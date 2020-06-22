/**
 * @file   larsim/PhotonPropagation/BinaryFilePhotonLibrary.h
 * @brief  Photon library whose data is read from a "flat" data file.
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date   June 4, 2020
 * @see    larsim/PhotonPropagation/BinaryFilePhotonLibrary.cxx
 * 
 * This is a copy of `phot::PhotonLibrary` object, where the visibility map for
 * direct light is stored in a plain binary file instead of the ROOT tree.
 * 
 */

#ifndef LARSIM_PHOTONPROPAGATION_BINARYFILEPHOTONLIBRARY_H
#define LARSIM_PHOTONPROPAGATION_BINARYFILEPHOTONLIBRARY_H

// LArSoft libraries
#include "larsim/PhotonPropagation/IPhotonLibrary.h"
#include "larsim/PhotonPropagation/VoxelizedChannelData.h"

// C/C++ standard libraries
#include <string>
#include <cstddef> // std::size_t


// -----------------------------------------------------------------------------
namespace phot { class BinaryFilePhotonLibrary; }

/**
 * @brief Simple photon library implementation with binary visibility sources.
 * 
 * This is a partial implementation of `phot::IPhotonLibrary` interface, which
 * supports the following features:
 * 
 * * visibility of source scintillation points: two maps are supported,
 *   that are usually referred to as "reflected" and... the other one
 * 
 * The following features are _not_ supported:
 * 
 * * reflected light first light timing
 * * direct light timing parametrization
 * 
 * Any attempt to use the interface for these unsupported features will throw
 * an exception.
 * 
 * @note While we refer to two libraries according to the terminology in
 *       @ref larsim_BinaryFilePhotonLibrary_glossary "the glossary below",
 *       the general idea is that this object supports multiple libraries
 *       (so far the interface fix the number to two), all contributing to
 *       the total visibility. The difference between the libraries is only
 *       beyond visibility: for example, they might have different arrival time
 *       distribution, but this does not affect this object: the caller will
 *       be able to have the visibility from the different components, and
 *       dealing with them as needed.
 * 
 * Note that the timing parametrization is provided in `larg4::LegacyLArG4`
 * with means different than this library.
 * 
 * Also note that this object *does not support building a library*.
 * In principle this can be changed; note however that there is no interface
 * to do that, therefore any code attempting to do that will have to either
 * introduce that interface in `phot::IPhotonLibrary` and
 * `phot::PhotonVisibilityService`, or to address this object directly.
 * 
 * So far, the supported way to build a library is to use `phot::PhotonLibrary`
 * implementation and then convert the library file.

 * 
 * 
 * Glossary
 * =========
 * 
 * @anchor larsim_BinaryFilePhotonLibrary_glossary
 * 
 * The following wording is used throughout this documentation:
 * 
 * * _direct light_: this is the main visibility map, accessed by a reflected
 *   light flag to `false`; existing libraries assign this library to describe
 *   the visibility of the scintillation light reaching the optical detectors
 *   without changing wave length;
 * * _reflected light_: this is the secondary visibility map, accessed by a
 *   reflected light flag to `true`; existing libraries assign this library to
 *   describe the visibility of the scintillation light reaching the optical
 *   detectors after a wave length change;
 * 
 * 
 * 
 * Notes on the implementation of building a library
 * ==================================================
 * 
 * Two ways are suggested for building the library.
 * Both utilize an interface similar to the one in `PhotonLibrary`.
 * 
 * In one case, the library can be allocated on disk fully, and on each voxel
 * either 0 or a computed value is written at the proper place in the file.
 * With the hope that the file system is ok with all that seeking.
 * 
 * In the other, a special format of file is implemented that holds only part
 * of the library, and then again on each voxel a computed value is written at
 * the proper place in the file.
 * 
 * In both cases, a post-processing job will have to merge the fragments, in the
 * first case adding all the data, as in the second case, where some more
 * optimization may be possible.
 * 
 */
class phot::BinaryFilePhotonLibrary: public phot::IPhotonLibrary {
    public:
  
  /**
   * @brief Constructor: loads the libraries from files.
   * @param DirectVisibilityPlainFilePath full path to the direct light library
   * @param ReflectedVisibilityPlainFilePath full path to the reflected light
   *        library
   * 
   * The libraries must have consistent sizes (number of voxels and of
   * channels).
   * 
   * The reflected light library path may be empty, in which case the library
   * is set not to support the reflected light visibility.
   */
  BinaryFilePhotonLibrary(
    std::string const& DirectVisibilityPlainFilePath,
    std::string const& ReflectedVisibilityPlainFilePath
    );

  // --- BEGIN -- Query interface ----------------------------------------------
  
  /// Returns the direct light visibility from `Voxel` to the `OpChannel`.
  virtual float GetCount(size_t Voxel, size_t OpChannel) const override;
  
  /// Returns the reflected light visibility from `Voxel` to the `OpChannel`.
  virtual float GetReflCount(size_t Voxel, size_t OpChannel) const override;

  /// Returns the direct light visibility from `Voxel` to all channels.
  virtual Counts_t GetCounts(size_t Voxel) const override;
  
  /// Returns the reflected light visibility from `Voxel` to all channels.
  virtual Counts_t GetReflCounts(size_t Voxel) const override;
  
  /// Returns whether the current library deals with reflected light count.
  virtual bool hasReflected() const override { return fHasReflected; }

  /// Returns whether the current library deals with reflected light timing
  /// (hint: it does not).
  virtual bool hasReflectedT0() const override { return false; }

  /// Returns the number of optical channels covered by this library.
  virtual int NOpChannels() const override { return fNOpChannels; }
  
  /// Returns the number of voxels covered by this library.
  virtual int NVoxels() const override { return fNVoxels; }
  
  /// Returns whether the specified `Voxel` is covered by this library.
  virtual bool isVoxelValid(size_t Voxel) const override
    { return isVoxelValidImpl(Voxel); }
  
  // --- END -- Query interface ------------------------------------------------


  // --- BEGIN -- Not implemented query interface ------------------------------
  /// @name Unsupported queries.
  /// @{
  
  virtual float GetReflT0(size_t, size_t) const override
    { NotImplemented("GetReflT0"); }
  
  virtual T0s_t GetReflT0s(size_t Voxel) const override
    { NotImplemented("GetReflT0s"); }
  
  /// @}
  // --- END -- Not implemented query interface --------------------------------
  
  
    private:
  
  /// Type of resource handler for the direct light visibility table.
  using LookupTableFile_t = phot::VoxelizedChannelData<float>;

  // --- BEGIN -- Data members -------------------------------------------------
  
  std::size_t fNVoxels;     ///< Number of covered voxels.
  std::size_t fNOpChannels; ///< Number of covered channels.

  /// Whether the current library deals with reflected light counts.
  bool fHasReflected = false;

  /// File-based lookup table for direct light.
  LookupTableFile_t fLookupTable;
  
  /// File-based lookup table for reflected light. Undefined state if
  /// `hasReflected()` is `false`.
  LookupTableFile_t fReflLookupTable;
  
  // --- END -- Data members ---------------------------------------------------
  
  
  /// Returns whether the specified `Voxel` is covered by this library.
  bool isVoxelValidImpl(size_t Voxel) const { return Voxel < fNVoxels; }

  /// Returns a "loaded" library from the specified file.
  LookupTableFile_t LoadLibraryFromPlainDataFile
    (std::string const& fileName) const;
  
  /// Returns the total number of entries in the direct light lookup table.
  std::size_t LookupTableSize() const;
  
  
  /// Reads the value at the specified `Voxel` and `OpChannel` from the `table.`
  float getTableCount
    (LookupTableFile_t const& table, std::size_t voxel, std::size_t opChannel)
    const;

  /// Reads the values at specified `Voxel` for all channels from the `table.`
  Counts_t getTableCounts
    (LookupTableFile_t const& table, std::size_t voxel) const;

  
  /**
   * @brief Throws an exception with not-implemented message.
   * @throw cet::exception (category `"BinaryFilePhotonLibrary"`) always thrown
   */
  [[noreturn]] static void NotImplemented(std::string const& funcName);
  
  
  
}; // phot::BinaryFilePhotonLibrary


// -----------------------------------------------------------------------------


#endif // LARSIM_PHOTONPROPAGATION_BINARYFILEPHOTONLIBRARY_H
