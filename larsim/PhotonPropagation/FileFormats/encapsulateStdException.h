/**
 * @file   larsim/PhotonPropagation/FileFormats/encapsulateStdException.h
 * @brief  Encapsulates a STL exception into a CET one.
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date   June 20, 2020
 * 
 * Header-only library
 */

#ifndef LARSIM_PHOTONPROPAGATION_FILEFORMATS_ENCAPSULATESTDEXCEPTION_H
#define LARSIM_PHOTONPROPAGATION_FILEFORMATS_ENCAPSULATESTDEXCEPTION_H


// framework libraries
#include "cetlib_except/exception.h"
#include "cetlib_except/demangle.h"

// C/C++ standard library
#include <string>
#include <typeinfo>
#include <exception>


// -----------------------------------------------------------------------------
namespace util {
  
  cet::exception encapsulateStdException(std::exception const& e);
  cet::exception encapsulateStdException
    (cet::exception::Category const& category, std::exception const& e);
  cet::exception encapsulateStdException(
    cet::exception::Category const& category,
    std::string msg,
    std::exception const& e
    );
  
} // namespace util


// -----------------------------------------------------------------------------
/**
 * @brief Encapsulates a STL exception into a `cet::exception`.
 * @param category _(default: `"StdException"`)_ category of wrapping exception
 * @param msg additional message to be appended
 * @param e the STL exception to be wrapped
 * @return a `cet::exception` with all the information from `e`
 * 
 * CET exceptions (`cet::exception`) provide an encapsulation mechanism so that
 * exceptions can be wrapped into others and propagated.
 * This mechanism does not cover STL exceptions (`std::exception`).
 * This function creates and returns a CET exception with as much information as
 * possible extracted from the STL exception `e`. This is not a real
 * encapsulation since the object `e` is not propagated nor copied.
 * 
 * As a special feature, if the exception `e` is a `cet::exception`, the
 * returned exception encapsulates `e` via the `cet::exception` encapsulation
 * mechanism.
 * 
 * Example of usage:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 * try {
 *   something();
 * }
 * catch (std::exception const& e) {
 *   
 *   throw encapsulateStdException("SomeTrier", e);
 *   
 * }
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * 
 * @note `cet::exception` is itself a `std::exception`, so the block in the
 *       example will catch also them.
 * 
 */
inline cet::exception util::encapsulateStdException(
  cet::exception::Category const& category,
  std::string msg,
  std::exception const& e
) {
  if (!msg.empty() && (msg.back() != '\n')) msg += '\n';
  
  std::string const& eName = cet::demangle_symbol(typeid(e).name());
  
  cet::exception const* ce = dynamic_cast<cet::exception const*>(&e);
  if (ce) {
    return
      { category, "CET exception of type " + eName + " wrapped.\n" + msg, *ce };
  }
  else {
    return {
      category,
      "STL exception of type " + eName + ":\n" + msg + e.what() + "\n"
      };
  }
  
} // util::encapsulateStdException()


// -----------------------------------------------------------------------------
/// Like `encapsulateStdException(cet::exception::Category const&, std::string, std:::exception const&)`
/// with category `"StdException"` and no additional message.
inline cet::exception util::encapsulateStdException(std::exception const& e)
  { return encapsulateStdException("StdException", e); }


// -----------------------------------------------------------------------------
/// Like `encapsulateStdException(cet::exception::Category const&, std::string, std:::exception const&)`
/// with no additional message.
inline cet::exception util::encapsulateStdException
  (cet::exception::Category const& category, std::exception const& e)
  { return encapsulateStdException(category, "", e); }


// -----------------------------------------------------------------------------


#endif // LARSIM_PHOTONPROPAGATION_FILEFORMATS_ENCAPSULATESTDEXCEPTION_H
