#ifndef __SETOPERATION_HPP__
#define __SETOPERATION_HPP__

/*
 * SetOperation
 *
 * Copyright Notice:
 * Copyright (C) 2020 Casa Systems.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or
 * object forms)
 * without the expressed written consent of Casa Systems.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CASA SYSTEMS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CASA
 * SYSTEMS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <algorithm>
#include <functional>
#include <set>
#include <type_traits>

namespace eqmi
{

template <typename T>
struct ContainerTraits
{
    typedef typename T::value_type ValueType;
};

template <typename T>
struct SortedContainerTraits : public ContainerTraits<T>
{
    typedef typename T::key_compare Compare;
};

template <typename T>
class SetOperation
{
  public:
    typedef T type;
    typedef typename SortedContainerTraits<T>::ValueType ValueType;
    typedef typename SortedContainerTraits<T>::Compare Compare;

    SetOperation() : lessCompare() {}

    T getDifference(const T &c1, const T &c2)
    {
        T retValue;
        std::set_difference(c1.cbegin(), c1.cend(), c2.cbegin(), c2.cend(), std::inserter(retValue, retValue.end()), lessCompare);

        return retValue;
    }

    T get()
    {
        return T();
    }

    template <typename T2>
    T doPtrTransfer(T2 &c)
    {
        using ValueType2 = typename ContainerTraits<T2>::ValueType;

        static_assert(std::is_same_v<ValueType, std::add_pointer_t<ValueType2>>, "object type not matching");
        T retValue;

        std::transform(c.begin(), c.end(), std::inserter(retValue, retValue.end()), [](ValueType2 &v2) {
            return &v2;
        });

        return retValue;
    }

  private:
    Compare lessCompare;
};

} // namespace eqmi

#endif
