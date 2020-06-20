/**
 * @file   larsim/PhotonPropagation/FileFormats/BinaryBlockFile.h
 * @brief  Defines utilities for the definition of binary file formats.
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date   June 7, 2020
 * @see    `larsim/PhotonPropagation/FileFormats/BinaryBlockFile.cxx`
 *         `larsim/PhotonPropagation/FileFormats/BinaryBlockFile.tcc`
 */

#ifndef LARSIM_PHOTONPROPAGATION_FILEFORMATS_BINARYBLOCKFILE_H
#define LARSIM_PHOTONPROPAGATION_FILEFORMATS_BINARYBLOCKFILE_H

// LArSoft libraries
#include "larsim/PhotonPropagation/FileFormats/FileBlocks.h" // util::fblock ns.

// framework libraries
#include "cetlib_except/exception.h"

// C/C++ standard library
#include <vector>
#include <array>
#include <fstream>
#include <ostream>
#include <string_view>
#include <string>
#include <type_traits> // std::remove_cv_t, std::conditional_t<>, ...
#include <cstddef> // std::size_t
#include <cstdint> // std::uint32_t


// -----------------------------------------------------------------------------
namespace util {
  
  namespace fblock {
  
    class BlockDescr;
    class BinaryBlockFile;
    
    std::ostream& operator<< (std::ostream& out, BlockDescr const& descr);
    
    namespace literals {
      BlockDescr operator ""_bd (char const* s, std::size_t n);
    } // namespace literals
    
    using namespace literals;
    
  } // namespace fblock
  
  using fblock::BinaryBlockFile;
  
} // namespace util


// -----------------------------------------------------------------------------
/// This is a string wrapper class for disambiguation of function arguments.
class util::fblock::BlockDescr {
  
  std::string fStr;
  
    public:
  BlockDescr() = default;
  explicit BlockDescr(char const* str): fStr(str) {}
  explicit BlockDescr(std::string const& str): fStr(str) {}
  explicit BlockDescr(std::string&& str): fStr(std::move(str)) {}
  
  std::string const& str() const { return fStr; }
  
  bool empty() const { return fStr.empty(); }
  auto size() const { return fStr.size(); }
  
}; // util::fblock::BlockDescr

inline std::ostream& util::fblock::operator<<
  (std::ostream& out, BlockDescr const& descr)
  { out << descr.str(); return out; }

inline auto util::fblock::literals::operator ""_bd
  (char const* str, std::size_t n) -> BlockDescr
  { return BlockDescr{ str }; }


// -----------------------------------------------------------------------------
/**
 * @brief I/O manager for files in block file format.
 * 
 * 
 * 
 * 
 */
class util::fblock::BinaryBlockFile {
  
  std::fstream fStream; ///< Stream for I/O.
  
    public:
  
  /**
   * @brief Constructor: creates and manages a stream.
   * @tparam Args types of arguments for the stream constructor
   * @param args arguments for the stream constructor
   * 
   * Currently the stream is `std::fstream`.
   */
  template <typename... Args>
  BinaryBlockFile(Args&&... args);
  
  // --- BEGIN -- Read data blocks ---------------------------------------------
  /// @name Read data blocks
  /// @{
  
  /**
   * @brief Reads a block of the specified type.
   * @tparam Block type of block to read (default: a generic `FileBlock`)
   * @param blockType name or description of the block used in error messages
   * @return the read block
   * @throw cet::exception (category: `BinaryBlockFile`) on error
   */
  template <typename Block = FileBlock>
  Block readBlock(BlockDescr const& blockType = {});

  /**
   * @brief Reads a block of the specified type and checks its key.
   * @tparam Block type of block to read (default: a generic `FileBlock`)
   * @param expectedKey the magic key that `block` is expected to have
   * @param blockType name or description of the block used in error messages
   * @return the read block
   * @throw cet::exception (category: `BinaryBlockFile`) on read error or key
   *                       mismatch
   */
  template <typename Block = FileBlock>
  Block readBlock
    (MagicKey const& expectedKey, BlockDescr const& blockType = {});
  
  /**
   * @brief Reads the header of the next block.
   * @param blockType name or description of the block used in error messages
   * @return the read block header
   * @throw cet::exception (category: `BinaryBlockFile`) on error
   */
  util::fblock::BlockInfo readBlockHeader(BlockDescr const& blockType = {});

  /**
   * @brief Reads the header of the next block and checks its key.
   * @param expectedKey the magic key that `block` is expected to have
   * @param blockType name or description of the block used in error messages
   * @return the read block header
   * @throw cet::exception (category: `BinaryBlockFile`) on read error or key
   *                       mismatch
   */
  util::fblock::BlockInfo readBlockHeader
    (MagicKey const& expectedKey, BlockDescr const& blockType = {});
  
  /**
   * @brief Reads the payload at the current position of the file.
   * @tparam Block type of block to read (default: a generic `FileBlock`)
   * @param info header of the current payload
   * @return the complete file block, including header and payload
   */
  template <typename Block = FileBlock>
  Block readPayload(BlockInfo const& info);
  
  
  /**
   * @brief Reads and returns a `Version` block at the current position in file.
   * @return the read `Version` block
   * @throw cet::exception on errors
   */
  util::fblock::Version readVersion();

  /**
   * @brief Reads and returns a `Version` block at the current position in file.
   * @param expectedKey the key for the version
   * @return the read `Version` block
   * @throw cet::exception on errors (also if read key is not the expected one)
   */
  util::fblock::Version readVersion(util::fblock::MagicKey const& expectedKey);


  /// @}
  // --- END -- Read data blocks -----------------------------------------------
  
  
  // --- BEGIN -- Skip data ----------------------------------------------------
  /// @name Skip data
  /// @{
  
  //@{
  /**
   * @brief Skips the next block of the file.
   * @param blockType name or description of the block used in error messages
   * @return the header of the skipped block
   * @throw cet::exception (category: `BinaryBlockFile`) on read error
   */
  BlockInfo skipBlock(BlockDescr const& blockType = {});
  //@}
  
  //@{
  /**
   * @brief Skips the next block of the file, with the specified key.
   * @param expectedKey the magic key that `block` is expected to have
   * @param blockType name or description of the block used in error messages
   * @return the header of the skipped block
   * @throw cet::exception (category: `BinaryBlockFile`) on read error or key
   *                       mismatch
   *
   */
  BlockInfo skipBlock
    (MagicKey const& expectedKey, BlockDescr const& blockType = {});
  //@}
  
  
  //@{
  /**
   * @brief Skips the payload at the current position of the file.
   * @param info header of the current payload
   * @param blockType name or description of the block used in error messages
   * @return `info` (moved from the argument)
   * @throw cet::exception (category: `BinaryBlockFile`) on read error
   */
  BlockInfo skipPayload(BlockInfo&& info, BlockDescr const& blockType = {});
  BlockInfo const& skipPayload
    (BlockInfo const& info, BlockDescr const& blockType = {});
  //@}
  
  /// @}
  // --- END -- Skip data ------------------------------------------------------
  
  
  // --- BEGIN -- Write data ---------------------------------------------------
  /// @name Write data
  /// @{
  
  /**
   * @brief Writes the specified block at the current position.
   * @tparam Block type of the block being written
   * @param block the block to be written
   * @return the written block (that is, `block`)
   */
  template <typename Block = FileBlock>
  std::enable_if_t<!std::is_same_v<std::decay_t<Block>, MagicKey>, Block const&>
  writeBlock(Block const& block);
  
  /**
   * @brief Writes the specified block at the current position.
   * @tparam Block type of the block being written
   * @param block the block to be written
   * @return the written block (that is, `block`)
   * 
   * If the argument is a (lvalue) reference, this method is not enabled.
   */
  template <typename Block = FileBlock>
  std::enable_if_t<
    !std::is_same_v<std::decay_t<Block>, MagicKey>
      && !std::is_lvalue_reference_v<Block>,
    Block>
  writeBlock(Block&& block);
  
  /**
   * @brief Constructs a block and writes it at the current position.
   * @tparam Block type of the block being written (mandatory)
   * @tparam Args types of arguments to `Block` constructor
   * @param args the other arguments of the constructor of `Block`
   * @return the written block
   */
  template <typename Block, typename... Args>
  Block writeBlock(MagicKey const& key, Args&&... args);
  
  /**
   * @brief Write a block with separate data.
   * @tparam T type of data being written (pretty much ignored)
   * @param blockInfo header of the block to be written
   * @param payload pointer to the data to be written
   * @return the header of the written block
   * @throw cet::exception (category: `BinaryBlockFile`) on write error
   * 
   * This approach to writing allows not to duplicate a payload that exists
   * already in memory just for that to be written into the file.
   */
  template <typename T>
  BlockInfo writeBlockAndPayload(BlockInfo const& blockInfo, T const* payload);
  
  
  /// @}
  // --- END -- Write data -----------------------------------------------------
  
  
  /// Returns the current reading position in the input stream.
  auto currentOffset() { return fStream.tellg(); }
  

    protected:
  
  /// Implementation of `writeBlock()` (l-value reference block version).
  template <typename Block>
  void writeBlockImpl(Block const& block);
  
  /**
   * @brief Checks that the specified block has the expected key.
   * @tparam Block type of block to test
   * @param blockType name or description of the block used in error messages
   * @param block the block to be tested
   * @param expectedKey the magic key that `block` is expected to have
   * @return `block` itself (moved from the argument if r-value reference)
   * @throw cet::exception (category: `BinaryBlockFile`) if the key does not
   *                       meet the expectation
   */
  template <typename Block>
  Block keyCheck(
    BlockDescr const& blockType,
    Block&& block,
    util::fblock::MagicKey const& expectedKey
    ) const;
  
  
}; // util::fblock::BinaryBlockFile



// -----------------------------------------------------------------------------
// ---  template implementation
// -----------------------------------------------------------------------------

#include "larsim/PhotonPropagation/FileFormats/BinaryBlockFile.tcc"

// -----------------------------------------------------------------------------


#endif // LARSIM_PHOTONPROPAGATION_FILEFORMATS_BINARYBLOCKFILE_H
