/**
 * @file   larsim/PhotonPropagation/BinaryFilePhotonLibrary.cxx
 * @brief  Photon library whose data is read from a "flat" data file.
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date   June 4, 2020
 * @see    larsim/PhotonPropagation/BinaryFilePhotonLibrary.h
 * 
 */

// library header
#include "larsim/PhotonPropagation/BinaryFilePhotonLibrary.h"

// LArSoft libraries
#include "larcorealg/CoreUtils/counter.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

// C/C++ standard libraries
#include <cassert>


// -----------------------------------------------------------------------------
phot::BinaryFilePhotonLibrary::BinaryFilePhotonLibrary(
  std::string const& DirectVisibilityPlainFilePath,
  std::string const& ReflectedVisibilityPlainFilePath
  )
  : fHasReflected(!ReflectedVisibilityPlainFilePath.empty())
  , fLookupTable(LoadLibraryFromPlainDataFile(DirectVisibilityPlainFilePath))
{
  if (fHasReflected) {
    fReflLookupTable
     = LoadLibraryFromPlainDataFile(ReflectedVisibilityPlainFilePath);
    if (fReflLookupTable.NData() != fLookupTable.NData()) {
      throw cet::exception("BinaryFilePhotonLibrary")
        << "Visibility maps for direct and reflected light have inconsistent size:"
        << "\n  direct: " << fLookupTable.NVoxels() << " voxels x "
          << fLookupTable.NChannels() << " channels => "
          << fLookupTable.NData()
        << "\n  reflected: " << fReflLookupTable.NVoxels() << " voxels x "
          << fReflLookupTable.NChannels() << " channels => "
          << fReflLookupTable.NData()
        << "\n";
    }
  } // if reflected
  
  fNVoxels = fLookupTable.NVoxels();
  fNOpChannels = fLookupTable.NChannels();
  
} // phot::BinaryFilePhotonLibrary::BinaryFilePhotonLibrary()


//------------------------------------------------------------------------------
auto phot::BinaryFilePhotonLibrary::LoadLibraryFromPlainDataFile
  (std::string const& fileName) const -> LookupTableFile_t
{
  
  LookupTableFile_t lookupTableFile { fileName };
  mf::LogVerbatim("BinaryFilePhotonLibrary")
    << "BinaryFilePhotonLibrary: loaded light map from '" << fileName
    << "' (" << lookupTableFile.NVoxels() << " voxels, "
    << lookupTableFile.NChannels() << " channels)";
  
  mf::LogTrace("BinaryFilePhotonLibrary")
    << "Library '" << fileName << "' metadata:\n" << lookupTableFile.metadata();
  
  return lookupTableFile;
} // BinaryFilePhotonLibrary::LoadLibraryFromPlainDataFile()


//------------------------------------------------------------------------------
float phot::BinaryFilePhotonLibrary::GetCount
  (size_t Voxel, size_t OpChannel) const
{
  return getTableCount(fLookupTable, Voxel, OpChannel);
} // phot::BinaryFilePhotonLibrary::GetCount()


//------------------------------------------------------------------------------
float phot::BinaryFilePhotonLibrary::GetReflCount
  (size_t Voxel, size_t OpChannel) const
{
  assert(fHasReflected); // if not, you are on your own
  return getTableCount(fReflLookupTable, Voxel, OpChannel);
} // phot::BinaryFilePhotonLibrary::GetReflCount()


//------------------------------------------------------------------------------
auto phot::BinaryFilePhotonLibrary::GetCounts(size_t Voxel) const -> Counts_t {
  return getTableCounts(fLookupTable, Voxel);
} // phot::BinaryFilePhotonLibrary::GetCounts()


//------------------------------------------------------------------------------
auto phot::BinaryFilePhotonLibrary::GetReflCounts(size_t Voxel) const
  -> Counts_t
{
  return getTableCounts(fReflLookupTable, Voxel);
} // phot::BinaryFilePhotonLibrary::GetCounts()


//------------------------------------------------------------------------------
std::size_t phot::BinaryFilePhotonLibrary::LookupTableSize() const {
  assert(fLookupTable.NData() == fReflLookupTable.NData());
  return fLookupTable.NData();
} // phot::BinaryFilePhotonLibrary::LookupTableSize()


//------------------------------------------------------------------------------
float phot::BinaryFilePhotonLibrary::getTableCount
  (LookupTableFile_t const& table, std::size_t voxel, std::size_t opChannel)
  const
{
  return (isVoxelValidImpl(voxel) && (opChannel < fNOpChannels))
    ? table.getDataAt(voxel, opChannel): 0;
} // phot::BinaryFilePhotonLibrary::GetCount()


//------------------------------------------------------------------------------
auto phot::BinaryFilePhotonLibrary::getTableCounts
  (LookupTableFile_t const& table, std::size_t voxel) const -> Counts_t
{
  
  if (!isVoxelValidImpl(voxel)) return nullptr;
  return { table.getDataAt(voxel) }; // pointer owns the data
  
} // phot::BinaryFilePhotonLibrary::getTableCounts()


//------------------------------------------------------------------------------
[[noreturn]] void phot::BinaryFilePhotonLibrary::NotImplemented
  (std::string const& funcName)
{
  throw cet::exception("BinaryFilePhotonLibrary")
    << "BinaryFilePhotonLibrary does not implement: " << funcName << "()\n";
} // phot::BinaryFilePhotonLibrary::NotImplemented()


//------------------------------------------------------------------------------
