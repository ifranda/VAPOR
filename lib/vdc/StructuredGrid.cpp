#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <time.h>
#ifdef Darwin
    #include <mach/mach_time.h>
#endif
#ifdef _WINDOWS
    #include "windows.h"
    #include "Winbase.h"
    #include <limits>
#endif

#include <vapor/utils.h>
#include <vapor/StructuredGrid.h>

using namespace std;
using namespace VAPoR;

void StructuredGrid::_StructuredGrid(const vector<size_t> &dims, const vector<size_t> &bs, const vector<float *> &blks)
{
    assert(bs.size() == 2 || bs.size() == 3);
    assert(bs.size() == dims.size());

    _bs.clear();
    _bdims.clear();
    _blks.clear();

    for (int i = 0; i < bs.size(); i++) {
        assert(bs[i] > 0);

        _bs.push_back(bs[i]);
        _bdims.push_back((dims[i] / bs[i]) + 1);
    }

    //
    // Shallow  copy blocks
    //
    _blks = blks;
}

StructuredGrid::StructuredGrid(const vector<size_t> &dims, const vector<size_t> &bs, const vector<float *> &blks) : Grid(dims, dims.size()) { _StructuredGrid(dims, bs, blks); }

StructuredGrid::~StructuredGrid() {}

float StructuredGrid::AccessIndex(const std::vector<size_t> &indices) const { return (_AccessIndex(_blks, indices)); }

float StructuredGrid::_AccessIndex(const std::vector<float *> &blks, const std::vector<size_t> &indices) const
{
    assert(indices.size() == GetTopologyDim());

    if (!blks.size()) return (GetMissingValue());

    vector<size_t> dims = GetDimensions();
    size_t         ndim = dims.size();
    for (int i = 0; i < ndim; i++) {
        if (indices[i] >= dims[i]) { return (GetMissingValue()); }
    }

    size_t xb = indices[0] / _bs[0];
    size_t yb = indices[1] / _bs[1];
    size_t zb = ndim == 3 ? indices[2] / _bs[2] : 0;

    size_t x = indices[0] % _bs[0];
    size_t y = indices[1] % _bs[1];
    size_t z = ndim == 3 ? indices[2] % _bs[2] : 0;

    float *blk = blks[zb * _bdims[0] * _bdims[1] + yb * _bdims[0] + xb];
    return (blk[z * _bs[0] * _bs[1] + y * _bs[0] + x]);
}

void StructuredGrid::GetRange(float range[2]) const
{
    StructuredGrid::ConstIterator itr;
    bool                          first = true;
    range[0] = range[1] = GetMissingValue();
    float missingValue = GetMissingValue();
    for (itr = this->begin(); itr != this->end(); ++itr) {
        if (first && *itr != missingValue) {
            range[0] = range[1] = *itr;
            first = false;
        }

        if (!first) {
            if (*itr < range[0] && *itr != missingValue)
                range[0] = *itr;
            else if (*itr > range[1] && *itr != missingValue)
                range[1] = *itr;
        }
    }
}

template<class T> StructuredGrid::ForwardIterator<T>::ForwardIterator(T *rg)
{
    if (!rg->_blks.size()) {
        _end = true;
        return;
    }

    vector<size_t> dims = rg->GetDimensions();
    vector<size_t> bs = rg->GetBlockSize();
    vector<size_t> bdims = rg->GetDimensionInBlks();
    assert(dims.size() > 1 && dims.size() < 4);
    for (int i = 0; i < dims.size(); i++) {
        _max[i] = dims[i] - 1;
        _bs[i] = bs[i] - 1;
        _bdims[i] = bdims[i];
    }

    _rg = rg;
    _x = 0;
    _y = 0;
    _z = 0;
    _xb = 0;
    _itr = &rg->_blks[0][0];
    _ndim = dims.size();
    _end = false;
}

template<class T> StructuredGrid::ForwardIterator<T>::ForwardIterator()
{
    _rg = NULL;
    _xb = 0;
    _x = 0;
    _y = 0;
    _z = 0;
    _itr = NULL;
    _end = true;
}

template<class T> StructuredGrid::ForwardIterator<T> &StructuredGrid::ForwardIterator<T>::operator++()
{
    if (!_rg->_blks.size()) _end = true;
    if (_end) return (*this);

    _xb++;
    _itr++;
    _x++;
    if (_xb < _bs[0] && _x < _max[0]) { return (*this); }

    _xb = 0;
    if (_x > _max[0]) {
        _x = _xb = 0;
        _y++;
    }

    if (_y > _max[1]) {
        if (_ndim == 2) {
            _end = true;
            return (*this);
        }
        _y = 0;
        _z++;
    }

    if (_ndim == 3 && _z > _max[2]) {
        _end = true;
        return (*this);
    }

    size_t xb = _x / _bs[0];
    size_t yb = _y / _bs[1];
    size_t zb = 0;
    if (_ndim == 3) zb = _z / _bs[2];

    size_t x = _x % _bs[0];
    size_t y = _y % _bs[1];
    size_t z = 0;
    if (_ndim == 3) z = _z % _bs[2];
    float *blk = _rg->_blks[zb * _bdims[0] * _bdims[1] + yb * _bdims[0] + xb];
    _itr = &blk[z * _bs[0] * _bs[1] + y * _bs[0] + x];
    return (*this);
}

template<class T> StructuredGrid::ForwardIterator<T> StructuredGrid::ForwardIterator<T>::operator++(int)
{
    if (_end) return (*this);

    ForwardIterator temp(*this);
    ++(*this);
    return (temp);
}

template<class T> StructuredGrid::ForwardIterator<T> &StructuredGrid::ForwardIterator<T>::operator+=(const long int &offset)
{
    _end = false;

    vector<size_t> min, max;
    for (int i = 0; i < _ndim; i++) {
        min.push_back(0);
        max.push_back(0);
    }

    vector<size_t> xyz;
    if (_ndim > 0) xyz.push_back(_x);
    if (_ndim > 1) xyz.push_back(_y);
    if (_ndim > 2) xyz.push_back(_z);

    long newoffset = Wasp::LinearizeCoords(xyz, min, max) + offset;

    size_t maxoffset = Wasp::LinearizeCoords(max, min, max);

    if (newoffset < 0 || newoffset > maxoffset) {
        _end = true;
        return (*this);
    }

    xyz = Wasp::VectorizeCoords(offset, min, max);
    _x = _y = _z = 0;

    if (_ndim > 0) _x = xyz[0];
    if (_ndim > 1) _y = xyz[1];
    if (_ndim > 2) _z = xyz[2];
    _xb = _x % _bs[0];

    size_t xb = _x / _bs[0];

    size_t yb = _y / _bs[1];
    size_t zb = 0;
    if (_ndim == 3) zb = _z / _bs[2];

    size_t x = _x % _bs[0];
    size_t y = _y % _bs[1];
    size_t z = 0;
    if (_ndim == 3) z = _z % _bs[2];

    float *blk = _rg->_blks[zb * _bdims[0] * _bdims[1] + yb * _bdims[0] + xb];
    _itr = &blk[z * _bs[0] * _bs[1] + y * _bs[0] + x];
    return (*this);
}

template<class T> StructuredGrid::ForwardIterator<T> StructuredGrid::ForwardIterator<T>::operator+(const long int &offset) const
{
    ForwardIterator temp(*this);

    if (_end) return (temp);

    temp += offset;
    return (temp);
}

template<class T> bool StructuredGrid::ForwardIterator<T>::operator!=(const StructuredGrid::ForwardIterator<T> &other)
{
    if (this->_end && other._end) return (false);

    return (!(this->_rg == other._rg && this->_xb == other._xb && this->_x == other._x && this->_y == other._y && this->_z == other._z && this->_itr == other._itr && this->_end == other._end));
}

// Need this so that template definitions can be made in .cpp file, not .h file
//
template class StructuredGrid::ForwardIterator<StructuredGrid>;
template class StructuredGrid::ForwardIterator<const StructuredGrid>;

namespace VAPoR {
std::ostream &operator<<(std::ostream &o, const StructuredGrid &sg)
{
    o << "StructuredGrid " << endl;
    o << " Block dimensions";
    for (int i = 0; i < sg._bs.size(); i++) { o << sg._bs[i] << " "; }
    o << endl;

    o << " Grid dimensions in blocks";
    for (int i = 0; i < sg._bdims.size(); i++) { o << sg._bdims[i] << " "; }
    o << endl;

    o << (const Grid &)sg;

    return o;
}
};    // namespace VAPoR
