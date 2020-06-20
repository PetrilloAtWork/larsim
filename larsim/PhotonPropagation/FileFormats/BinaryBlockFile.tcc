/**
 * @file   larsim/PhotonPropagation/FileFormats/BinaryBlockFile.tcc
 * @brief  Defines utilities for the definition of binary file formats.
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date   June 7, 2020
 * @see    `larsim/PhotonPropagation/FileFormats/BinaryBlockFile.h`
 * 
 * This is a template implementation file.
 */

#ifndef LARSIM_PHOTONPROPAGATION_FILEFORMATS_BINARYBLOCKFILE_TCC
#define LARSIM_PHOTONPROPAGATION_FILEFORMATS_BINARYBLOCKFILE_TCC


#ifndef LARSIM_PHOTONPROPAGATION_FILEFORMATS_BINARYBLOCKFILE_H
# error "This header must not be included directly; include `larsim/PhotonPropagation/FileFormats/BinaryBlockFile.h` instead"
#endif // !LARSIM_PHOTONPROPAGATION_FILEFORMATS_BINARYBLOCKFILE_H


// LArSoft libraries
#include "larcorealg/CoreUtils/DebugUtils.h" // lar::debug::demangle()

// framework libraries
#include "cetlib_except/exception.h"

// C/C++ standard library
#include <algorithm> // std::copy_n()
#include <iterator> // std::distance(), std::next()
#include <utility> // std::move()


// -----------------------------------------------------------------------------
// ---  template implementation
// -----------------------------------------------------------------------------
template <typename... Args>
util::fblock::BinaryBlockFile::BinaryBlockFile(Args&&... args)
  : fStream(std::forward<Args>(args)...)
{
  fStream.exceptions
    (std::fstream::failbit | std::fstream::badbit | std::fstream::eofbit);
  fStream.seekg(0);
} // util::fblock::BinaryBlockFile::BinaryBlockFile()


// -----------------------------------------------------------------------------
template <typename Block>
Block util::fblock::BinaryBlockFile::readBlock
  (BlockDescr const& blockType /* = {} */)
{
  
  Block block;
  
  if (!block.read(fStream)) {
    throw cet::exception("BinaryBlockFile")
      << "Failed to read "
      << (blockType.empty()? lar::debug::demangle<Block>(): blockType.str())
      << ".\n";
  }
  
  return block;
  
} // util::fblock::BinaryBlockFile::readBlock()


// -----------------------------------------------------------------------------
template <typename Block>
Block util::fblock::BinaryBlockFile::readBlock
  (MagicKey const& expectedKey, BlockDescr const& blockType /* = {} */)
{
  return keyCheck(blockType, readBlock<Block>(blockType), expectedKey);
} // util::fblock::BinaryBlockFile::readBlock()


// -----------------------------------------------------------------------------
template <typename Block /* = FileBlock */>
auto
/* Block const& */ util::fblock::BinaryBlockFile::writeBlock(Block const& block)
  -> std::enable_if_t<!std::is_same_v<std::decay_t<Block>, MagicKey>, Block const&>
{
  writeBlockImpl(block);
  return block;
} // util::fblock::BinaryBlockFile::writeBlock(Block const&)


// -----------------------------------------------------------------------------
template <typename Block /* = FileBlock */>
// std::enable_if_t<!std::is_lvalue_reference_v<Block>, Block>
auto
util::fblock::BinaryBlockFile::writeBlock(Block&& block)
  -> std::enable_if_t<
    !std::is_same_v<std::decay_t<Block>, MagicKey>
      && !std::is_lvalue_reference_v<Block>,
    Block>
{
  writeBlockImpl(block);
  return std::move(block);
} // util::fblock::BinaryBlockFile::writeBlock(Block&&)


// -----------------------------------------------------------------------------
template <typename Block, typename... Args>
Block util::fblock::BinaryBlockFile::writeBlock
  (MagicKey const& key, Args&&... args)
{
  Block block { key, std::forward<Args>(args)... };
  writeBlockImpl(block);
  return block;
} // util::fblock::BinaryBlockFile::writeBlock()


// -----------------------------------------------------------------------------
template <typename T>
auto util::fblock::BinaryBlockFile::writeBlockAndPayload
  (BlockInfo const& blockInfo, T const* payload) -> BlockInfo
{
  if (!util::fblock::writeBlockAndPayload(fStream, blockInfo, payload)) {
    throw cet::exception("BinaryBlockFile")
      << "writeBlockAndPayload(): error writing block '" << blockInfo.key()
      << "' (" << blockInfo.size() << " bytes)!\n";
  }
  return blockInfo;
} // util::fblock::BinaryBlockFile::writeBlockAndPayload()


// -----------------------------------------------------------------------------
template <typename Block = util::fblock::FileBlock>
Block util::fblock::BinaryBlockFile::readPayload(BlockInfo const& info) {
  Block block{ info };
  block.readPayload(fStream);
  return block;
} // util::fblock::BinaryBlockFile::readPayload()


// -----------------------------------------------------------------------------
template <typename Block>
void util::fblock::BinaryBlockFile::writeBlockImpl(Block const& block) {
  
  if (!block.write(fStream)) {
    throw cet::exception("BinaryBlockFile")
      << "Failed to write block '" << block.key() << "' (type: "
      << lar::debug::demangle<Block>() << ")"
      ;
  }
  
} // util::fblock::BinaryBlockFile::writeBlockImpl()


// -----------------------------------------------------------------------------
template <typename Block>
Block util::fblock::BinaryBlockFile::keyCheck(
  BlockDescr const& blockType, Block&& block, MagicKey const& expectedKey
  ) const
{
  if (!block.hasKey(expectedKey)) {
    throw cet::exception("BinaryBlockFile")
      << blockType << " has key '" << block.key() << "' (expected: '"
      << expectedKey << "')\n";
  }
  return std::move(block);
} // util::fblock::BinaryBlockFile::keyCheck()


// -----------------------------------------------------------------------------


#endif // LARSIM_PHOTONPROPAGATION_FILEFORMATS_BINARYBLOCKFILE_TCC
