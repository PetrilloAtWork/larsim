/**
 * @file   larsim/PhotonPropagation/PhotonLibraryBinaryFileFormat.cxx
 * @brief  Photon library whose data is read from a "flat" data file.
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date   June 4, 2020
 * @see    larsim/PhotonPropagation/PhotonLibraryBinaryFileFormat.h
 * 
 * This is a copy of `phot::PhotonLibrary` object, where the visibility map for
 * direct light is stored in a plain binary file instead of the ROOT tree.
 * 
 */

/// library header
#include "larsim/PhotonPropagation/PhotonLibraryBinaryFileFormat.h"

// LArSoft libraries
#include "larsim/PhotonPropagation/FileFormats/BinaryBlockFile.h"
#include "larcorealg/CoreUtils/enumerate.h"
#include "larcorealg/CoreUtils/RealComparisons.h"

// framework libraries
#include "cetlib_except/exception.h"

// C/C++ standard libraries
#include <fstream>
#include <string>
#include <iomanip>
#include <cctype> // std::toupper()



// -----------------------------------------------------------------------------
namespace {
  
  constexpr std::array AxisNames = { 'x', 'y', 'z' };
  
} // local namespace


// -----------------------------------------------------------------------------
auto phot::PhotonLibraryBinaryFileFormat::readHeader()
  -> HeaderSettings_t const&
{
  
  util::BinaryBlockFile srcFile = openLibraryFile();
  
  fHeader.emplace();
  auto& header = fHeader->header;
  
  try {
    //
    // the weird _bd strings are literal util::fblock::BlockDescr objects:
    // syntax candy from util::fblock::literals
    //
    using namespace util::fblock::literals;
    namespace fblock = util::fblock;
  
    
    using UIntBlock_t = fblock::Number<unsigned int>;
    using DoubleBlock_t = fblock::Number<double>;
    
    header.version = srcFile.readVersion("PLIB").version();
    
    if (header.version == 0) {
      throw cet::exception("PhotonLibraryBinaryFileFormat")
        << "Photon library file " << fLibraryPath
        << " appears to be of unsupported version " << header.version
        << ".\n";
    }
    
    /*
     * The following implementation is very hard-coded;
     * in principle using `readBlockHeader()` we can read any format sporting
     * the elements that we support (and optionally warning about the others).
     */
    
    if (header.version > LatestFormatVersion) {
      throw cet::exception("PhotonLibraryBinaryFileFormat")
        << "Photon library file " << fLibraryPath
        << " appears to be of version " << header.version
        << ", which this software does not support.\n";
    }
    
    /*
     * Version 1
     */
    header.configuration =
     srcFile.readBlock<fblock::String>("CNFG", "configuration string"_bd).str();
    
    header.nEntries = srcFile.readBlock<UIntBlock_t>
      ("NTRY", "number of entries in the table"_bd).value();
    
    header.nChannels = srcFile.readBlock<UIntBlock_t>
      ("NCHN", "number of channels in the table"_bd).value();
    
    header.nVoxels = srcFile.readBlock<UIntBlock_t>
      ("NVXL", "number of voxels in the table"_bd).value();
    
    for (auto [ iAxis, axis ]: util::enumerate(header.axes)) {
      
      using fblock::BlockDescr;
      
      std::string const letter { (char) AxisNames[iAxis] };
      std::string const Letter { (char) std::toupper(AxisNames[iAxis]) };
      
      srcFile.readBlock<fblock::Bookmark>
        ("AXI" + Letter, BlockDescr{ letter + " axis segmentation" });
      
      axis.nSteps = srcFile.readBlock<UIntBlock_t>
        ("NBO" + Letter, BlockDescr{ letter + " axis" }).value();
      axis.lower = srcFile.readBlock<DoubleBlock_t>
        ("MIN" + Letter, BlockDescr{ letter + " range lower bound" }).value();
      axis.upper = srcFile.readBlock<DoubleBlock_t>
        ("MAX" + Letter, BlockDescr{ letter + " range upper bound" }).value();
      axis.step = srcFile.readBlock<DoubleBlock_t>
        ("STE" + Letter, BlockDescr{ letter + " range step size" }).value();
      
      srcFile.readBlock<fblock::Bookmark>
        ("END" + Letter, BlockDescr{ letter + " axis data end" });
      
    } // for
    
    auto const dataBlockInfo
      = srcFile.readBlockHeader("PHVS", "photon visibility data (header)"_bd);
    if (dataBlockInfo.sizeAs<float>() != header.nEntries) {
      throw cet::exception("PhotonLibraryBinaryFileFormat")
        << "Expected " << header.nEntries << " x " << sizeof(float) << " = "
        << (header.nEntries * sizeof(float))
        << " bytes of visibility data, data block is " << dataBlockInfo.size()
        << " bytes instead.\n";
    }
    fHeader->dataOffset = srcFile.currentOffset();
    srcFile.skipPayload(dataBlockInfo, "photon visibility data"_bd);
    
    srcFile.readBlock<fblock::Bookmark>("DONE", "visibility data end"_bd);
    
  }
  catch (std::exception const& e) {
    
    fHeader.reset();
    
    throw util::encapsulateStdException("PhotonLibraryBinaryFileFormat", e)
      << "PhotonLibraryBinaryFileFormat::readHeader(): error reading file "
      << fLibraryPath << "\n";
    
  }
  
  return header;
  
} // phot::PhotonLibraryBinaryFileFormat::readHeader()


// -----------------------------------------------------------------------------
void phot::PhotonLibraryBinaryFileFormat::writeHeader() const {
  
  util::BinaryBlockFile destFile = writeLibraryFile(std::ios::trunc);
  
  try {
    writeHeader(destFile);
  }
  catch (std::exception const& e) {
    
    throw util::encapsulateStdException("PhotonLibraryBinaryFileFormat", e)
      << "PhotonLibraryBinaryFileFormat::writeHeader(): error reading file "
      << fLibraryPath << "\n";
    
  }
  
} // phot::PhotonLibraryBinaryFileFormat::writeHeader()


// -----------------------------------------------------------------------------
void phot::PhotonLibraryBinaryFileFormat::setHeader
  (HeaderSettings_t const& header)
{
  fHeader.emplace();
  fHeader->header = header;
  fixHeader();
} // phot::PhotonLibraryBinaryFileFormat::setHeader()


void phot::PhotonLibraryBinaryFileFormat::setHeader(HeaderSettings_t&& header) {
  fHeader.emplace();
  fHeader->header = std::move(header);
  fixHeader();
} // phot::PhotonLibraryBinaryFileFormat::setHeader()


// -----------------------------------------------------------------------------
void phot::PhotonLibraryBinaryFileFormat::writeFooter() const {
  
  util::BinaryBlockFile destFile = writeLibraryFile(std::ios::app);
  
  try {
    writeFooter(destFile);
  }
  catch (std::exception const& e) {
    
    throw util::encapsulateStdException("PhotonLibraryBinaryFileFormat", e)
      << "PhotonLibraryBinaryFileFormat::writeFooter(): error reading file "
      << fLibraryPath << "\n";
    
  }
  
} // phot::PhotonLibraryBinaryFileFormat::writeFooter()


// -----------------------------------------------------------------------------
void phot::PhotonLibraryBinaryFileFormat::writeHeader
  (util::BinaryBlockFile& destFile) const
{
  
  namespace fblock = util::fblock;
  
  if (!fHeader) {
    throw cet::exception("PhotonLibraryBinaryFileFormat")
      << "phot::PhotonLibraryBinaryFileFormat::writeHeader() "
      " attempted to write a file header without any header information!\n";
  }
  
  auto const& header = fHeader->header;
  
  try {
    
    destFile.writeBlock<fblock::Version>("PLIB", header.version);
    
    using UIntBlock_t = fblock::Number<unsigned int>;
    using DoubleBlock_t = fblock::Number<double>;
    
    /*
     * Version 1
     */
    destFile.writeBlock<fblock::String>("CNFG", header.configuration);
    
    destFile.writeBlock<UIntBlock_t>("NTRY", header.nEntries);
    
    destFile.writeBlock<UIntBlock_t>("NCHN", header.nChannels);
    
    destFile.writeBlock<UIntBlock_t>("NVXL", header.nVoxels);
    
    for (auto const& [ iAxis, axis ]: util::enumerate(header.axes)) {
      
      std::string const Letter { (char) std::toupper(AxisNames[iAxis]) };
      
      destFile.writeBlock<fblock::Bookmark>("AXI" + Letter);
      
      destFile.writeBlock<UIntBlock_t>  ("NBO" + Letter, axis.nSteps);
      destFile.writeBlock<DoubleBlock_t>("MIN" + Letter, axis.lower);
      destFile.writeBlock<DoubleBlock_t>("MAX" + Letter, axis.upper);
      destFile.writeBlock<DoubleBlock_t>("STE" + Letter, axis.step);
      
      destFile.writeBlock<fblock::Bookmark>("END" + Letter);
      
    } // for
    
  }
  catch (std::exception const& e) {
    
    throw util::encapsulateStdException("PhotonLibraryBinaryFileFormat", e)
      << "PhotonLibraryBinaryFileFormat::writeHeader(): error writing file!\n";
    
  }
  
} // phot::PhotonLibraryBinaryFileFormat::writeHeader()


// -----------------------------------------------------------------------------
void phot::PhotonLibraryBinaryFileFormat::writeFooter
  (util::BinaryBlockFile& destFile) const
{
  
  try {
    
    /*
     * Version: 1
     */
    destFile.writeBlock<util::fblock::Bookmark>("DONE");
    
  }
  catch (std::exception const& e) {
    
    throw util::encapsulateStdException("PhotonLibraryBinaryFileFormat", e)
      << "PhotonLibraryBinaryFileFormat::writeFooter(): error writing file!\n";
    
  }
  
} // phot::PhotonLibraryBinaryFileFormat::writeFooter(BinaryBlockFile)


// -----------------------------------------------------------------------------
void phot::PhotonLibraryBinaryFileFormat::dumpInfo(std::ostream& out) const {
  
  out << "Library file " << fLibraryPath;
  if (!fHeader) {
    out << " not loaded.\n";
    return;
  }
  
  out << fHeader->header
    << "Visibility data starts at file offset: 0x" << std::hex
    << fHeader->dataOffset;
  
  out << "\n";
  
} // phot::PhotonLibraryBinaryFileFormat::dumpInfo()


// -----------------------------------------------------------------------------
util::BinaryBlockFile phot::PhotonLibraryBinaryFileFormat::writeLibraryFile
  (std::ios::openmode mode /* = std::ios::binary | std::ios::out | std::ios::app */)
  const
{
  
  try {
    
    if (fLibraryPath.has_relative_path()) {
      try {
        std::filesystem::create_directories(fLibraryPath.parent_path());
      }
      catch (std::exception const& e) {
        throw util::encapsulateStdException("PhotonLibraryBinaryFileFormat", e)
          << "phot::PhotonLibraryBinaryFileFormat::writeLibraryFile(): "
          "error creating the path " << fLibraryPath.parent_path()
          << " for the photon library file " << fLibraryPath.filename()
          << " (see encapsulated exceptions for the details)\n"
          ;
      }
    } // if has directory
    
    return util::BinaryBlockFile
      { fLibraryPath, mode | std::ios::binary | std::ios::out };
  }
  catch (std::exception const& e) {
    
    throw util::encapsulateStdException("PhotonLibraryBinaryFileFormat", e)
      << "phot::PhotonLibraryBinaryFileFormat::writeLibraryFile(): "
      "error creating photon library file " << fLibraryPath
      << " (see encapsulated exceptions for the details)\n"
      ;
    
  }
  
} // phot::PhotonLibraryBinaryFileFormat::writeLibraryFile()


// -----------------------------------------------------------------------------
util::BinaryBlockFile phot::PhotonLibraryBinaryFileFormat::openLibraryFile
  (std::ios::openmode mode /* = std::ios::binary | std::ios::in */) const
{
  
  try {
    return util::BinaryBlockFile
      { fLibraryPath, mode | std::ios::binary | std::ios::in };
  }
  catch (std::exception const& e) {
    
    throw util::encapsulateStdException("PhotonLibraryBinaryFileFormat", e)
      << "phot::PhotonLibraryBinaryFileFormat::openLibraryFile(): "
      "error opening photon library file " << fLibraryPath
      << " (see encapsulated exceptions for the details)\n"
      ;
    
  }
  
} // phot::PhotonLibraryBinaryFileFormat::openLibraryFile()


// -----------------------------------------------------------------------------
void phot::PhotonLibraryBinaryFileFormat::fixHeader() {
  
  constexpr lar::util::RealComparisons cmp { 1e-3 };
  
  if (!fHeader) {
    throw cet::exception("PhotonLibraryBinaryFileFormat")
      << "phot::PhotonLibraryBinaryFileFormat::fixHeader(): "
      " header not present!!\n";
  }
  
  auto& header = fHeader->header;
  
  // set the default version if requested
  if (header.version == DefaultFormatVersion)
    header.version = LatestFormatVersion;
  
  //
  // axes check
  //
  unsigned int nVoxels = 1U;
  for (auto const& [ iAxis, axis ]: util::enumerate(header.axes)) {
    
    nVoxels *= axis.nSteps;
    
    //
    // boundary check
    //
    auto const expectedUpper = axis.lower + axis.step * axis.nSteps;
    if (cmp.nonEqual(axis.upper, expectedUpper)) {
      throw cet::exception("PhotonLibraryBinaryFileFormat")
        << "fixHeader(): inconsistent information: axis " << AxisNames[iAxis]
        << " with " << axis.nSteps << " x " << axis.step << " cm from "
        << axis.lower << " cm should end at " << expectedUpper << " cm, not at "
        << axis.upper << "!\n";
    } // if upper mismatch
    
  } // for axes
  
  //
  // voxel number
  //
  if (nVoxels != header.nVoxels) {
    throw cet::exception("PhotonLibraryBinaryFileFormat")
      << "fixHeader(): inconsistent information: axes tell about " << nVoxels
      << " voxels, but total number is set to " << header.nVoxels << "!\n";
  }
  
  //
  // total entries
  //
  auto const nEntries = header.nVoxels * header.nChannels;
  if (nEntries != header.nEntries) {
    throw cet::exception("PhotonLibraryBinaryFileFormat")
      << "fixHeader(): inconsistent information: " << header.nChannels
      << " channels for " << header.nVoxels << " voxels should make "
      << nEntries << " entries, not " << header.nEntries << "!\n";
  }
  
} // phot::PhotonLibraryBinaryFileFormat::fixHeader()


// -----------------------------------------------------------------------------
std::ostream& phot::operator<< (
  std::ostream& out,
  phot::PhotonLibraryBinaryFileFormat::HeaderSettings_t const& header
) {
  
  std::string const unit = "cm";
  
  out << " (format version " << header.version << ")"
    << "\n  " << header.nEntries << " entries = " << header.nVoxels
      << " voxels x " << header.nChannels << " channels"
    ;
  for (auto const& [ iAxis, axis ]: util::enumerate(header.axes)) {
    
    char const letter = AxisNames[iAxis];
    out << "\n  " << letter << " axis: [ " << axis.lower << " -- " << axis.upper
      << " ] " << unit << " in " << axis.nSteps << " steps, "
      << axis.step << " " << unit << " each"
      ;
    
  } // for axes
  out << "\n";
  
  return out;
} // phot::operator<<(HeaderSettings_t)


// -----------------------------------------------------------------------------

