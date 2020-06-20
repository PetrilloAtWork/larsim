/**
 * @file larsim/PhotonPropagation/FileFormats/BinaryBlockFile.cxx
 * @brief  Defines utilities for the definition of binary file formats.
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date   June 7, 2020
 * @see `larsim/PhotonPropagation/FileFormats/BinaryBlockFile.h`
 */

// library header
#include "larsim/PhotonPropagation/FileFormats/BinaryBlockFile.h"

// framework libraries
#include "cetlib_except/exception.h"

// C/C++ standard library
#include <algorithm> // std::copy_n(), std::copy(), std::fill(), std::min()
#include <ostream>
#include <utility> // std::move()


// -----------------------------------------------------------------------------
// ---  util::BinaryBlockFile
// -----------------------------------------------------------------------------
auto util::fblock::BinaryBlockFile::readVersion() -> Version {
  return readBlock<Version>("version block"_bd);
} // util::fblock::BinaryBlockFile::readVersion()


// -----------------------------------------------------------------------------
auto util::fblock::BinaryBlockFile::readVersion(MagicKey const& expectedKey)
  -> Version
{
  return readBlock<Version>(expectedKey, "version block"_bd);
} // util::fblock::BinaryBlockFile::readVersion()


// -----------------------------------------------------------------------------
auto util::fblock::BinaryBlockFile::readBlockHeader
  (BlockDescr const& blockType /* = {} */) -> BlockInfo
{
  // kind of cheating since this is not really a block but just its header
  return readBlock<BlockInfo>
    (blockType.empty()? "block header"_bd: blockType);
} // util::fblock::BinaryBlockFile::readBlockHeader()


// -----------------------------------------------------------------------------
auto util::fblock::BinaryBlockFile::readBlockHeader
  (MagicKey const& expectedKey, BlockDescr const& blockType /* = {} */)
  -> BlockInfo 
{
  // kind of cheating since this is not really a block but just its header
  return readBlock<BlockInfo>
    (expectedKey, blockType.empty()? "block header"_bd: blockType);
} // util::fblock::BinaryBlockFile::readBlockHeader(key)


// -----------------------------------------------------------------------------
auto util::fblock::BinaryBlockFile::skipBlock
  (BlockDescr const& blockType /* = {} */) -> BlockInfo 
{
  return skipPayload(readBlockHeader(blockType), blockType);
} // util::fblock::BinaryBlockFile::skipBlock()


// -----------------------------------------------------------------------------
auto util::fblock::BinaryBlockFile::skipBlock
  (MagicKey const& expectedKey, BlockDescr const& blockType /* = {} */)
  -> BlockInfo
{
  return skipPayload(readBlockHeader(expectedKey, blockType), blockType);
} // util::fblock::BinaryBlockFile::skipBlock(key)


// -----------------------------------------------------------------------------
auto util::fblock::BinaryBlockFile::skipPayload
  (BlockInfo const& header, BlockDescr const& blockType /* = {} */)
  -> BlockInfo const&
{
  if (!header.skipPayload(fStream)) {
    throw cet::exception("BinaryBlockFile")
      << "Error while skipping " << header.alignedSize()
      << " bytes of data for "
      << (blockType.empty()? std::string(header.key()): blockType.str())
      << "\n";
  }
  return header;
} // util::fblock::BinaryBlockFile::skipPayload(key)


// -----------------------------------------------------------------------------
auto util::fblock::BinaryBlockFile::skipPayload
  (BlockInfo&& header, BlockDescr const& blockType /* = {} */) -> BlockInfo
{
  skipPayload(header, blockType); // l-value reference version called this time
  return std::move(header);
} // util::fblock::BinaryBlockFile::skipPayload(key)


// -----------------------------------------------------------------------------

