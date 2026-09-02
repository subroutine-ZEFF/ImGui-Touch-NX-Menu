/*
 * Copyright (c) Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <common.hpp>
#include <utility>
#include <new>
#include <memory>
#include <type_traits>
#include "aligned_storage.hpp"

namespace exl::util {

    template<typename T, size_t Size = sizeof(T), size_t Align = alignof(T)>
    struct TypedStorage {
        typename AlignedStorage<Size, Align>::Type _storage;
    };

    template<typename T>
    static ALWAYS_INLINE T *GetPointer(TypedStorage<T> &ts) {
        return std::launder(reinterpret_cast<T *>(std::addressof(ts._storage)));
    }

    template<typename T>
    static ALWAYS_INLINE const T *GetPointer(const TypedStorage<T> &ts) {
        return std::launder(reinterpret_cast<const T *>(std::addressof(ts._storage)));
    }

    template<typename T>
    static ALWAYS_INLINE T &GetReference(TypedStorage<T> &ts) {
        return *GetPointer(ts);
    }

    template<typename T>
    static ALWAYS_INLINE const T &GetReference(const TypedStorage<T> &ts) {
        return *GetPointer(ts);
    }

    namespace impl {

        /* NOTE: strip const so std::construct_at (which takes a non-const void*
           internally since GCC 15's libstdc++) accepts TypedStorage<const T>. */
        template<typename T>
        static ALWAYS_INLINE std::remove_const_t<T> *GetPointerForConstructAt(TypedStorage<T> &ts) {
            return reinterpret_cast<std::remove_const_t<T> *>(std::addressof(ts._storage));
        }

    }

    template<typename T, typename... Args>
    static ALWAYS_INLINE T *ConstructAt(TypedStorage<T> &ts, Args &&... args) {
        return std::construct_at(impl::GetPointerForConstructAt(ts), std::forward<Args>(args)...);
    }

    template<typename T>
    static ALWAYS_INLINE void DestroyAt(TypedStorage<T> &ts) {
        return std::destroy_at(GetPointer(ts));
    }

}