/**
 * @file   larsim/PhotonPropagation/FileFormats/details/initializeConstexprArray.h
 * @brief  Some craziness to initialize constexpr arrays. C++20 wanted.
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date   June 19, 2020
 */

#ifndef LARSIM_PHOTONPROPAGATION_FILEFORMATS_DETAILS_INITIALIZECONSTEXPRARRAY_H
#define LARSIM_PHOTONPROPAGATION_FILEFORMATS_DETAILS_INITIALIZECONSTEXPRARRAY_H


// C/C++ standard libraries
#include <array>
#include <cstddef> // std::size_t


// -----------------------------------------------------------------------------
// ---  interface
// -----------------------------------------------------------------------------
namespace util::details {
  
  /**
   * @brief Returns a `std::array<T, N>` with all elements set to `Value`.
   * @tparam N size of the array
   * @tparam T type of the array elements
   * @tparam Value (type: `T`) value to be assigned to all elements
   * @return a `std::array<T, N>` with all elements set to `Value`.
   * 
   * With C++20 this should become trivial...
   */
  template <typename T, std::size_t N, T Value>
  constexpr auto fillConstexprArray();
  
  
} // namespace util::details


// -----------------------------------------------------------------------------
// ---  template implementation
// -----------------------------------------------------------------------------
namespace util::details {

  
  // ---------------------------------------------------------------------------
  template <typename T, T... Values>
  struct StaticArray {
    
    static constexpr std::size_t size() { return sizeof...(Values); }
    
    static constexpr std::array<T, size()> makeArray() { return { Values... }; }
    
  }; // struct StaticArray


  // ---------------------------------------------------------------------------
  template <typename T>
  struct StaticArrayFarmer;

  // grows an array
  template <typename T, T FirstValue, T... OtherValues>
  struct StaticArrayFarmer<StaticArray<T, FirstValue, OtherValues...>> {
    
    using type = StaticArray<T, FirstValue, OtherValues...>;
    
    using self_type = StaticArrayFarmer<type>;
    
    template <std::size_t More>
    static constexpr auto grow()
      {
        if constexpr(More == 0) return self_type{};
        else {
          return StaticArrayFarmer
            <StaticArray<T, FirstValue, FirstValue, OtherValues...>>
            ::template grow<(More-1U)>();
        }
      } // grow()
    
    template <std::size_t More>
    using grown_type = decltype(self_type::grow<More>());
    
    template <std::size_t More>
    using grown_t = typename grown_type<More>::type;
    
  }; // StaticArrayFarmer<>


  // ---------------------------------------------------------------------------
  template <typename T, std::size_t N, T Value>
  constexpr auto fillConstexprArray() {
    
    static_assert(N >= 1U, "Static array size starts at 1.");
    
    using StaticArrayN = typename StaticArrayFarmer<StaticArray<T, Value>>
      ::template grown_t<(N-1)>;
    return StaticArrayN::makeArray();
    
  } // fillConstexprArray()
  
  // ---------------------------------------------------------------------------

} // namespace util::details



#endif // LARSIM_PHOTONPROPAGATION_FILEFORMATS_DETAILS_INITIALIZECONSTEXPRARRAY_H
