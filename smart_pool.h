#ifndef SMART_POOL_LIBRARY_H
#define SMART_POOL_LIBRARY_H


#include "chunk_pool.h"

#include <sstream>
#include <vector>
#include <cmath>
#include <stdexcept>

namespace util {
    template<class T>
    class smart_pool;

    template<class T>
    class smart_pool{
    private:

        const size_t _defaultSize;
        const size_t _blockSize;

        const size_t _pointerSize;
        const size_t _sizerSize;
        const size_t _headerSize;

        mutable std::vector<chunk_pool *> _chunks;

        mutable void *_head;

        void _newChunk()const {
            auto *chunk = new chunk_pool(static_cast<size_t>(std::exp2(std::ceil(std::log2(static_cast<float>(_blockSize * _defaultSize))))));
            _chunks.push_back(chunk);

            void *tmp = chunk->pointer();
            *(void **)tmp = _head;
            *(size_t *)((char *)tmp + _pointerSize) = chunk->size() - _headerSize;

            _head = tmp;

        }

    public:

        explicit smart_pool<T> (size_t size = 16384) : _defaultSize(size), _blockSize(sizeof(T)),
                                                       _pointerSize(sizeof(void *)), _sizerSize(sizeof(size_t)), _headerSize(_pointerSize + _sizerSize),
                                                       _chunks(), _head(nullptr) {}
        ~smart_pool<T> (){
            for (auto chunk : _chunks) {
                delete chunk;
            }
        }


        void* alloc(size_t count = 1)const {
            //TODO: lock(this)
            if (count <= 0) {
                std::stringstream str;
                str << "count less or equal zero: " << count;
                throw std::invalid_argument(str.str());
            }

            size_t needSize = count * _blockSize;

            void *targetPrev = nullptr;
            void *target = nullptr;
            size_t targetSize = 0xffffffff;

            void *prev = nullptr;
            void *tmp = _head;
            size_t tmpSize;

            while (tmp) {
                tmpSize = *(size_t *)((char *)tmp + _pointerSize);

                if (tmpSize >= needSize && tmpSize < targetSize) {
                    targetPrev = prev;
                    target = tmp;
                    targetSize = tmpSize;
                }

                if (tmpSize == needSize)
                    break;

                prev = tmp;
                tmp = *(void **)tmp;
            }

            if (!target) {
                _newChunk();
                target = _head;
                targetSize = *(size_t *)((char *)_head + _pointerSize);
                if (needSize > targetSize) {
                    throw std::bad_alloc();
                }
            }

            if (targetSize - needSize >= _blockSize + _headerSize) {
                //откалываем нужный кусок
                void *newTmp = (void *)((char *)target + _headerSize + needSize);
                if (_head == target) {
                    _head = newTmp;
                } else {
                    *(void **)targetPrev = newTmp;
                }
                *(void **)newTmp = *(void **)target;
                *(void **)target = nullptr;
                *(size_t *)((char *)newTmp + _pointerSize) = targetSize - needSize - _headerSize;

                *(size_t *)((char *)target + _pointerSize) = needSize;

                return (void *)((char *)target + _headerSize);
            } else {
                //все отдаем
                if (_head == target) {
                    _head = *(void **)target;
                } else {
                    *(void **)targetPrev = *(void **)target;
                }
                *(void **)target = nullptr;

                return (void *)((char *)target + _headerSize);
            }
        }
        void free(void *ptr)const {
            //TODO: lock(this)

            void *target = (void *)((char *)ptr - _headerSize);
            size_t targetSize = *(size_t *)((char *)ptr - _sizerSize);
            void *targetNext = (void *)((char *)ptr + targetSize);

            bool fromHead = false;
            bool fromTail = false;

            void *tmp = _head;
            void *prev = nullptr;

            while (tmp) {
                if (!fromTail && tmp == targetNext) {

                    targetSize = targetSize + _headerSize + *(size_t *) ((char *) targetNext + _pointerSize);
                    *(size_t *) ((char *) target + _pointerSize) = targetSize;

                    if (_head == targetNext) {
                        _head = *(void **) targetNext;
                    } else {
                        *(void **) prev = *(void **) targetNext;
                    }

                    fromTail = true;

                    if (fromHead) {
                        break;
                    }
                } else {
                    if (!fromHead) {
                        size_t prevSize = *(size_t *) ((char *) tmp + _pointerSize);
                        void *fromPrev = (void *) ((char *) tmp + _headerSize + prevSize);

                        if (fromPrev == target) {
                            if (_head == tmp) {
                                _head = *(void **) tmp;
                            } else {
                                *(void **) prev = *(void **) tmp;
                            }

                            target = tmp;
                            targetSize = targetSize + _headerSize + prevSize;

                            *(size_t *) ((char *) tmp + _pointerSize) = targetSize;

                            fromHead = true;
                            if (fromTail) {
                                break;
                            }
                        }
                    }
                }

                prev = tmp;
                tmp = *(void **)tmp;
            }

            *(void **) target = _head;
            _head = target;
        }

        bool is_mine(void *ptr)const {
            for (auto chunk : _chunks) {
                if (chunk->is_mine(ptr))
                    return true;
            }
            return false;
        }

    };

}

#endif