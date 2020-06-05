/**
 * @file   larsim/PhotonPropagation/CarefreePointerTraitsForOpDetVisibilityData.h
 * @brief  Definitions to use `phot::OpDetVisibilityData` with
 *         `util::CarefreePointer`.
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date   June 3, 2020
 *
 * The purpose of this header is to allow the inclusion of the types needed
 * for interacting with `phot::PhotonVisibilityService` without having to
 * include the full service.
 *
 * This is a header-only library.
 */

#ifndef LARSIM_PHOTONPROPAGATION_CAREFREEPOINTERTRAITSFOROPDETVISIBILITYDATA_H
#define LARSIM_PHOTONPROPAGATION_CAREFREEPOINTERTRAITSFOROPDETVISIBILITYDATA_H


// LArSoft libraries
#include "larsim/PhotonPropagation/LibraryMappingTools/OpDetVisibilityData.h"
#include "lardataalg/Utilities/CarefreePointer.h"


//------------------------------------------------------------------------------
namespace phot {
  
  //----------------------------------------------------------------------------
  template <typename... Args>
  struct LibraryDataValidatorStruct<util::CarefreePointer<Args...>>
    : public LibraryDataValidatorStruct
      <typename util::CarefreePointer<Args...>::element_type*>
  {};
  
  
  //----------------------------------------------------------------------------
  
  
} // namespace phot




//------------------------------------------------------------------------------


#endif // LARSIM_PHOTONPROPAGATION_CAREFREEPOINTERTRAITSFOROPDETVISIBILITYDATA_H
