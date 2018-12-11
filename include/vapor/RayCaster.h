#ifndef RAYCASTER_H
#define RAYCASTER_H

#include <GL/glew.h>
#include <sys/time.h>

#ifdef Darwin
    #include <OpenGL/gl.h>
    #include <OpenGL/glu.h>
#else
    #include <GL/gl.h>
    #include <GL/glu.h>
#endif

#include "vapor/Visualizer.h"
#include "vapor/RayCasterParams.h"
#include "vapor/DataMgr.h"
#include "vapor/DataMgrUtils.h"
#include "vapor/Grid.h"
#include "vapor/utils.h"
#include "vapor/GLManager.h"

#include <glm/glm.hpp>

namespace VAPoR {

class RENDER_API RayCaster : public Renderer {
public:
    RayCaster(const ParamsMgr *pm, std::string &winName, std::string &dataSetName, std::string paramsType, std::string classType, std::string &instName, DataMgr *dataMgr);

    virtual ~RayCaster();

protected:
    // C++ stuff
    // pure virtual functions from Renderer
    int _initializeGL();
    int _paintGL(bool fast);

    // Makes RayCaster an abstract class that cannot be instantiated,
    //   and it's up to the subclasses to decide which shader to load.
    //   It returns 0 upon success, and non-zero upon errors.
    virtual int _loadShaders() = 0;

    enum CastingMode { FixedStep = 1, CellTraversal = 2 };

    class UserCoordinates {
        // Note: class UserCoordinates lives completely inside of class RayCaster,
        //   and is solely used by class RayCaster. Thus for simplicity, it has all
        //   of its member variables and methods public.
    public:
        //              Y
        //              |   Z (coming out the screen)
        //              |  /
        //              | /
        //              |/
        //            0 --------X
        float *        frontFace, *backFace;    // user coordinates, size == bx * by * 3
        float *        rightFace, *leftFace;    // user coordinates, size == by * bz * 3
        float *        topFace, *bottomFace;    // user coordinates, size == bx * bz * 3
        float *        dataField;               // data field of this volume
        unsigned char *missingValueMask;        // 0 == is missing value; non-zero == not missing value
        float *        xyCoords;                // X-Y coordinate values
        float *        zCoords;                 // Z coordinate values

        size_t dims[4];    // num. of samples along each axis.
                           // !! Note: the last element is the diagnal length !!

        //             0---------2
        //              |       |
        //              |       |
        //              |       |
        //             1|_______|3
        // Also keep the coordinates of 4 vertices of the the near clipping plane.
        float nearCoords[12];

        /* Also keep the current meta data */
        size_t      myCurrentTimeStep;
        std::string myVariableName;
        int         myRefinementLevel, myCompressionLevel;
        float       myBoxMin[3], myBoxMax[3];    // Retrieved from params, instead of grid.

        /* Member functions */
        UserCoordinates();
        ~UserCoordinates();

        //
        // It returns 0 upon success, and non-zero upon errors.
        //
        int GetCurrentGrid(const RayCasterParams *params, DataMgr *dataMgr, StructuredGrid **gridpp) const;

        bool IsMetadataUpToDate(const RayCasterParams *params, DataMgr *dataMgr) const;
        //
        // Update meta data, as well as pointers: 6 faces + dataField + missingValueMask
        //   It returns 0 upon success, and non-zero upon errors:
        //     1 == Failed to get a StructuredGrid
        //    -1 == Failed to allocate memory for the 3D variable or missing value mask
        //
        int  UpdateFaceAndData(const RayCasterParams *params, DataMgr *dataMgr);
        void FillCoordsXYPlane(const StructuredGrid *grid,        // Input
                               size_t                planeIdx,    // Input: which plane to retrieve
                               float *               coords);                    // Output buffer allocated by caller
        void FillCoordsYZPlane(const StructuredGrid *grid,        // Input
                               size_t                planeIdx,    // Input
                               float *               coords);                    // Output
        void FillCoordsXZPlane(const StructuredGrid *grid,        // Input
                               size_t                planeIdx,    // Input
                               float *               coords);                    // Output
        //
        // Update pointers: xyCoords and zCoords
        // |-- Note: meta data is updated in UpdateFaceAndData(), but *NOT* here, so
        // |         UpdateFaceAndData() needs to be called priori to UpdateCurviCoords().
        // |-- It returns 0 upon success, and non-zero upon errors:
        //     |--  1 == Failed to get a StructuredGrid
        //     |-- -1 == Failed to allocate memory for the Z-coords
        //
        int UpdateCurviCoords(const RayCasterParams *params, DataMgr *dataMgr);
    };    // end of struct UserCoordinates

    UserCoordinates    _userCoordinates;
    std::vector<float> _colorMap;
    float              _colorMapRange[3];    // min, max, and diff values.

    // OpenGL stuff
    // textures
    GLuint       _backFaceTextureId;
    GLuint       _frontFaceTextureId;
    GLuint       _volumeTextureId;
    GLuint       _missingValueTextureId;
    GLuint       _colorMapTextureId;
    GLuint       _xyCoordsTextureId;
    GLuint       _zCoordsTextureId;
    const GLuint _backFaceTexOffset;
    const GLuint _frontFaceTexOffset;
    const GLuint _volumeTexOffset;
    const GLuint _colorMapTexOffset;
    const GLuint _missingValueTexOffset;
    const GLuint _xyCoordsTexOffset;
    const GLuint _zCoordsTexOffset;

    // buffers
    GLuint _frameBufferId;
    GLuint _xyCoordsBufferId;
    GLuint _zCoordsBufferId;
    GLenum _drawBuffers[2];    // Draw buffers for the 1st and 2nd pass

    // vertex arrays
    GLuint _vertexArrayId;
    GLuint _vertexBufferId;    // Keeps user coordinates of 6 faces.
    GLuint _indexBufferId;     // Auxiliary indices for efficiently drawing triangle strips.
    GLuint _vertexAttribId;    // Attribute of vertices: (i, j k) logical indices.

    // shaders
    GLuint _1stPassShaderId;
    GLuint _2ndPassShaderId;
    GLuint _3rdPassShaderId;
    GLuint _3rdPassMode1ShaderId;
    GLuint _3rdPassMode2ShaderId;
    GLint  _currentViewport[4];    // current viewport in use

    //
    // Render the volume surface using triangle strips
    //   This is a subroutine used by _drawVolumeFaces().
    //
    void _renderTriangleStrips(int whichPass, long castingMode) const;

    void _drawVolumeFaces(int whichPass, long whichCastingMode, bool insideACell = false, const glm::mat4 &inversedMV = glm::mat4(0.0f), bool fast = false);

    void _load3rdPassUniforms(long castingMode, const glm::mat4 &inversedMV, bool fast) const;

    virtual void _3rdPassSpecialHandling(bool fast, long castingMode);

    //
    // Initialization for 1) framebuffers and 2) textures
    //   It returns 0 upon success, and non-zero upon errors.
    //
    int _initializeFramebufferTextures();

    double _getElapsedSeconds(const struct timeval *begin, const struct timeval *end) const;

    void _updateViewportWhenNecessary();
    void _updateColormap(RayCasterParams *params);
    void _updateDataTextures(int castingMode);
    void _updateNearClippingPlane();
    void _enableVertexAttribute(const float *buf, size_t length, bool attrib1Enabled) const;

};    // End of class RayCaster

};    // End of namespace VAPoR

#endif
