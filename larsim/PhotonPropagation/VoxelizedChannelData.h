/**
 * @file   larsim/PhotonPropagation/VoxelizedChannelData.h
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date   July 1, 2020
 *
 * This is a header-only library.
 */

#ifndef LARSIM_PHOTONPROPAGATION_VOXELIZEDCHANNELDATA_H
#define LARSIM_PHOTONPROPAGATION_VOXELIZEDCHANNELDATA_H

#define LARSIM_PHOTONPROPAGATION_VOXELIZEDCHANNELDATA_MULTITHREADING_ACCESS 1


// LArSoft libraries
#include "larsim/PhotonPropagation/PhotonLibraryBinaryFileFormat.h"

// framework libraries
#include "cetlib_except/exception.h"

// C/C++ standard libraries
#include <mutex>
#include <filesystem>
#include <memory> // std::unique_ptr<>
#include <fstream>
#include <sstream>
#include <string>
#include <cassert>


// -----------------------------------------------------------------------------
namespace phot{ template <typename T> class VoxelizedChannelData; }

/**
 * @brief Reads data from a file, indexed by voxel and channel number.
 * @param T type of data in the file
 * 
 * This class reads from the proper position of a binary file the requested data
 * on each and every query.
 * No caching is performed. This comes with a great cost in execution time,
 * moderate CPU time and negligible memory overhead.
 * 
 */
template <typename T>
class phot::VoxelizedChannelData {
  
  //
  // the mutability shit here is because this is a test;
  // all this stuff should be not marked as const,
  // but the interface using it currently requires that
  //
  
  std::istream::pos_type fNVoxels; ///< Number of voxels.
  std::istream::pos_type fNChannels; ///< Number of channels per voxel.
  
  std::streamsize fVoxelDataSize; ///< Bytes of data per voxel.
  
#ifdef LARSIM_PHOTONPROPAGATION_VOXELIZEDCHANNELDATA_MULTITHREADING_ACCESS
  std::unique_ptr<std::mutex> fDataMutex; ///< Control of file (`fData`) access.
#endif
  mutable std::ifstream fData; ///< Data file.
  
  std::istream::pos_type fDataOffset; ///< Where in data file the data starts.
  
  /// Candy: keep the full metadata information from the source file.
  phot::PhotonLibraryBinaryFileFormat::HeaderSettings_t fMetadata;
  
  /// Returns the index in the file of the specified voxel data.
  std::size_t getIndex(unsigned int voxel) const;
  
  /// Returns the index in the file of the specified channel in voxel
  std::size_t getIndex(unsigned int voxel, unsigned int channel) const;
  
  /// Returns the position in the file of the specified data index.
  std::istream::pos_type getPosition(std::size_t index) const;
  
  
    public:
  using Data_t = T; ///< Type of contained data.
  
  /// Type of metadata record.
  using Metadata_t = phot::PhotonLibraryBinaryFileFormat::HeaderSettings_t;
  
  /// Sets this object in an undefined status. Assign it before using it!
  VoxelizedChannelData() = default;
  
  /// Initializes from the specified file and information.
  VoxelizedChannelData(std::filesystem::path const& fileName);
  
  /// Returns the number of stored voxels.
  unsigned int NVoxels() const { return static_cast<unsigned int>(fNVoxels); }
  
  /// Returns the number of channels stored for each voxel.
  unsigned int NChannels() const
    { return static_cast<unsigned int>(fNChannels); }
  
  /// Returns the total number of data entries.
  std::size_t NData() const { return NVoxels() * NChannels(); }
  
  /// Returns the metadata information.
  Metadata_t const& metadata() const { return fMetadata; }
  
  /// Fills the buffer with data for the specified `voxel` from all channels.
  /// @return a pointer to the filled buffer (that is, `data`)
  /// @bug This method is not thread-safe (but it is marked `const`).
  Data_t* fillWithDataAt(Data_t* data, unsigned int voxel) const;
  
  /// Allocates a new buffer with data for the specified `voxel` from all channels.
  /// @note This method is not thread-safe (but it is marked `const`).
  std::unique_ptr<Data_t[]> getDataAt(unsigned int voxel) const;
  
  /// Reads and returns the value for the specified `voxel` and `channel`.
  Data_t getDataAt(unsigned int voxel, unsigned int channel) const;
  
  
  /// Fills the `buffer` with `n` `Data_t` values starting with the one with
  /// the specified `index`.
  /// @return the filled buffer (that is, `buffer`)
  /// @note This method is not thread-safe (but it is marked `const`).
  Data_t* readData(Data_t* buffer, std::size_t index, std::size_t n = 1U) const;
  
  
  /// Returns an uninitialized buffer large enough for all data of one voxel.
  std::unique_ptr<Data_t[]> makeBuffer() const;
  
  
}; // class phot::VoxelizedChannelData<>


// -----------------------------------------------------------------------------
// ---  Template implementation
// -----------------------------------------------------------------------------
template <typename T>
std::size_t phot::VoxelizedChannelData<T>::getIndex(unsigned int voxel) const {
  return voxel * fNChannels;
} // phot::VoxelizedChannelData<T>::getIndex(voxel)


// -----------------------------------------------------------------------------
template <typename T>
std::size_t phot::VoxelizedChannelData<T>::getIndex
  (unsigned int voxel, unsigned int channel) const
{
  return getIndex(voxel) + static_cast<std::size_t>(channel);
} // phot::VoxelizedChannelData<T>::getIndex(channel)


// -----------------------------------------------------------------------------
template <typename T>
std::istream::pos_type phot::VoxelizedChannelData<T>::getPosition
  (std::size_t index) const
{
  return
    fDataOffset + static_cast<std::istream::pos_type>(index * sizeof(Data_t));
} // phot::VoxelizedChannelData<T>::getPosition(index)


// -----------------------------------------------------------------------------
template <typename T>
phot::VoxelizedChannelData<T>::VoxelizedChannelData
  (std::filesystem::path const& fileName)
  #ifdef LARSIM_PHOTONPROPAGATION_VOXELIZEDCHANNELDATA_MULTITHREADING_ACCESS
  : fDataMutex(std::make_unique<std::mutex>())
  #endif
{
  
  try {
    
    phot::PhotonLibraryBinaryFileFormat srcFile(fileName);
    
    fMetadata = srcFile.readHeader();
    assert(srcFile.hasHeader());
    
    fDataOffset = srcFile.dataOffset();
    
  }
  catch (cet::exception const& e) {
    throw cet::exception("VoxelizedChannelData", "", e)
      << "VoxelizedChannelData(): error while reading metadata from "
      << fileName << "\n";
  }
  
  fNVoxels = fMetadata.nVoxels;
  fNChannels = fMetadata.nChannels;
  fVoxelDataSize = fNChannels * sizeof(Data_t);
  
  fData.open(fileName, std::ios::binary);
  fData.exceptions(~std::ifstream::goodbit);
  if (!fData.is_open()) {
    throw cet::exception("VoxelizedChannelData")
      << "Failed to open visibility data file " << fileName << "\n";
  }

} // VoxelizedChannelData::VoxelizedChannelData()


// -----------------------------------------------------------------------------
template <typename T>
auto phot::VoxelizedChannelData<T>::fillWithDataAt
  (Data_t* data, unsigned int voxel) const -> Data_t*
{
  
  return readData(data, getIndex(voxel), fNChannels);
  
} // phot::VoxelizedChannelData<T>::fillWithDataAt()


// -----------------------------------------------------------------------------
template <typename T>
auto phot::VoxelizedChannelData<T>::getDataAt(unsigned int voxel) const
  -> std::unique_ptr<Data_t[]>
{
  auto buffer = makeBuffer();
  fillWithDataAt(buffer.get(), voxel);
  return buffer;
} // phot::VoxelizedChannelData<T>::getDataAt(voxel)


// -----------------------------------------------------------------------------
template <typename T>
auto phot::VoxelizedChannelData<T>::getDataAt
  (unsigned int voxel, unsigned int channel) const -> Data_t
{
  
  Data_t data;
  readData(&data, getIndex(voxel, channel));
  return data;
  
} // phot::VoxelizedChannelData<T>::getDataAt(voxel, channel)


// -----------------------------------------------------------------------------
template <typename T>
auto phot::VoxelizedChannelData<T>::makeBuffer() const
  -> std::unique_ptr<Data_t[]>
  { return std::make_unique<Data_t[]>(NChannels()); }


// -----------------------------------------------------------------------------
template <typename T>
auto phot::VoxelizedChannelData<T>::readData
  (Data_t* buffer, std::size_t index, std::size_t n /* = 1U */) const
  -> Data_t*
{
  // error handling is delegated to fData,
  // which should be set to throw an exception on error
  
#ifdef LARSIM_PHOTONPROPAGATION_VOXELIZEDCHANNELDATA_MULTITHREADING_ACCESS
  std::lock_guard lock(*fDataMutex);
#endif // LARSIM_PHOTONPROPAGATION_VOXELIZEDCHANNELDATA_MULTITHREADING_ACCESS
  // LOCK here
  fData.seekg(getPosition(index));
  fData.read(reinterpret_cast<char*>(buffer), n * sizeof(Data_t));
  // UNLOCK here
  
  return buffer;
  
} // phot::VoxelizedChannelData<T>::readData()


// -----------------------------------------------------------------------------


#endif // LARSIM_PHOTONPROPAGATION_VOXELIZEDCHANNELDATA_H
