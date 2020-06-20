/**
 * @file   larsim/PhotonPropagation/PhotonLibraryBinaryFileFormat.h
 * @brief  Photon library whose data is read from a "flat" data file.
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date   June 4, 2020
 * @see    larsim/PhotonPropagation/PhotonLibraryBinaryFileFormat.cxx
 * 
 * This is a copy of `phot::PhotonLibrary` object, where the visibility map for
 * direct light is stored in a plain binary file instead of the ROOT tree.
 * 
 */

#ifndef LARSIM_PHOTONPROPAGATION_PHOTONLIBRARYBINARYFILEFORMAT_H
#define LARSIM_PHOTONPROPAGATION_PHOTONLIBRARYBINARYFILEFORMAT_H

// LArSoft libraries
#include "larsim/PhotonPropagation/FileFormats/BinaryBlockFile.h"
#include "larsim/PhotonPropagation/FileFormats/encapsulateStdException.h"
#include "larcorealg/CoreUtils/enumerate.h"

// framework libraries
#include "cetlib_except/exception.h"

// C/C++ standard libraries
#include <string>
#include <array>
#include <vector>
#include <fstream>
#include <filesystem>
#include <optional>
#include <cstddef> // std::size_t


// -----------------------------------------------------------------------------
namespace phot { class PhotonLibraryBinaryFileFormat; }

/**
 * @brief Helper to read and write a photon library as flat binary file.
 * 
 * This object defines a file format for a photon visibility library with
 * a single table and two indices: a voxel number and a channel number.
 * 
 * 
 * File format
 * ============
 * 
 * The file format is a custom "file block" format which is made of "blocks".
 * All blocks include a ("magic") key that is 4 character long.
 * Then, each entry except the "version" has the size of the data in the block
 * after the size information.
 * Note that is this size is _S_ the next block lies a number of bytes away that
 * is _S_ _rounded up_ to align to 4 byte words (e.g. if a block is a string
 * "hello", with 5 characters, _S_ is `5` and the net block starts `8` bytes
 * after the storage of _S_).
 * The version is a special tag which has always 4 bytes after the magic key.
 * 
 * 
 * Versions: 1
 * ------------
 * 
 * * version (key `"PLIB"`, 32-bit integer): `1`
 * * configuration block (key `"CNFG"`, string): the full FHiCL configuration of
 *   the caller (usually `PhotonVisibilityService`)
 * * number of entries in the library (key: `"NTRY", 32-bit unsigned integer)
 * * channels in each voxel (key: `"NCHN"`, 32-bit unsigned integer)
 * * voxels in the full library (key: `"NVXL"`, 32-bit unsigned integer)
 * * X axis information block (key: `"AXIX")
 * * Y axis information block (key: `"AXIY")
 * * Z axis information block (key: `"AXIZ")
 * * visibility information (key: `"PHVS"`, single precision real numbers): all
 *   the visibility information, starting with voxel `0` channel `0`, then voxel
 *   `0` channel `1`, and so on
 * * end bookmark (key: `"DONE"`)
 * 
 * The axis information block for the _x_ axis is as follow:
 * * number of cells (key `"NBOX"`, 32-bit unsigned integer)
 * * lower coordinate (key `"MINX"`, double precision real number):
 *     expected to be in centimeters and, if meaningful, in world coordinates
 * * upper coordinate (key `"MAXX"`, double precision real number):
 *     expected to be in centimeters and, if meaningful, in world coordinates
 * * cell width (key: `"STEX"`, double precision real number):
 *     is unsurprisingly `(MAXX - MINX) / NBOX)`
 * * end block (key: `"ENDX"`): a bookmark
 * 
 * The axis information for _y_ and _z_ has the same pattern as for the _x_ axis
 * but all the keys end in `Y` or `Z` instead of `X`.
 * 
 * 
 */
class phot::PhotonLibraryBinaryFileFormat {
  
    public:
  
  using Version_t = unsigned int; ///< Type used to represent a version number.
  
  /// Placeholder for the default version (that is the `LatestFormatVersion`).
  static constexpr Version_t DefaultFormatVersion
    = std::numeric_limits<Version_t>::max();
  
  /// Undefined version number.
  static constexpr Version_t UndefinedFormatVersion { 0U };
  
  /// The latest supported version.
  static constexpr Version_t LatestFormatVersion { 1U };
  
  /// Library settings from the header.
  struct HeaderSettings_t {
    
    /// Setting of a single dimension.
    struct AxisSpecs_t {
      
      /// Number of steps the dimension is split into.
      unsigned int nSteps { 0U};
      
      double lower; ///< Lower bound of the covered range (world coords) [cm]
      double upper; ///< Upper bound of the covered range (world coords) [cm]
      double step; ///< Step size [cm]
      
    }; // AxisSpecs_t
    
    
    Version_t version = UndefinedFormatVersion; ///< Version of the file format.
    
    std::string configuration; ///< Configuration used for generation (FHiCL).
    
    unsigned int nEntries { 0U }; ///< Number of cells in the library.
    
    unsigned int nChannels { 0U }; ///< Number of channels covered.
    
    unsigned int nVoxels { 0U }; ///< Number of voxels present.
    
    /// Information on each of the dimensions.
    std::array<AxisSpecs_t, 3U> axes;
    
  }; // HeaderSettings_t
  
  
  /// Associates this object with the specified file. The file is not open yet.
  PhotonLibraryBinaryFileFormat(std::filesystem::path const& libraryPath);
  
  
  // --- BEGIN --- Access to metadata ------------------------------------------
  /// @name Access to metadata
  /// @{
  
  /// Returns whether header information is present at all.
  bool hasHeader() const { return fHeader.has_value(); }
  
  /// Returns the header information. Undefined if `hasHeader()` is `false`.
  HeaderSettings_t const& header() const { return fHeader->header; }
  
  /// Returns the position of first byte of the visibility data in the file.
  /// Undefined behaviour if `hasHeader()` is `false`.
  std::fstream::pos_type dataOffset() const { return fHeader->dataOffset; }
  
  /// @}
  // --- END --- Access to metadata --------------------------------------------
  
  
  // --- BEGIN -- Input/output -------------------------------------------------
  /// @name Input/output
  /// @{
  
  /**
   * @brief Reads the header of the file.
   * @throw std::exception (from C++ standard library) on I/O errors
   * @throw cet::exception on format errors
   * 
   * If this call is successful, the header will have been parsed and the object
   * will know about the configuration, the axis extents and the number of
   * channels and voxels. It also knows the offset from the beginning of the
   * file to the visibility data.
   */
  HeaderSettings_t const& readHeader();
  
  
  /**
   * @brief Appends the information in the header into the file.
   * @throw std::exception (from C++ standard library) on I/O errors
   * @throw cet::exception on format errors
   */
  void writeHeader() const;
  
  
  // @{
  /// Imports the settings from the specified `header` information.
  void setHeader(HeaderSettings_t const& header);
  void setHeader(HeaderSettings_t&& header);
  // @}
  
  
  /**
   * @brief Appends all the `data` to the library file.
   * @tparam T type of data being written
   * @param data the data to be written
   * @throw cet::exception (category: `PhotonLibraryBinaryFileFormat`) on error
   */
  template <typename T>
  void writeData(std::vector<T> const& data) const;
  
  
  /**
   * @brief Appends a closing marker to the file.
   * @throw std::exception (from C++ standard library) on I/O errors
   * @throw cet::exception on format errors
   */
  void writeFooter() const;
  
  /**
   * @brief Writes the whole file (header, `data` and footer) to the file.
   * @tparam T type of data being written
   * @param data the data to be written
   * @throw cet::exception (category: `PhotonLibraryBinaryFileFormat`) on error
   */
  template <typename T>
  void writeFile(std::vector<T> const& data) const;

  /// @}
  // --- END -- Input/output ---------------------------------------------------
  
  
  /// Prints information about the table.
  void dumpInfo(std::ostream& out) const;
  
  
    protected:
  
  /// Write the header into the specified file.
  void writeHeader(util::BinaryBlockFile& destFile) const;
  
  /// Write all the `data` into the specified file.
  template <typename T>
  void writeData
    (util::BinaryBlockFile& destFile, std::vector<T> const& data) const;
  
  /// Write a closing marker into the specified file.
  void writeFooter(util::BinaryBlockFile& destFile) const;
  
  /// Writes the whole file (header, `data` and footer) to the file.
  template <typename T>
  void writeFile
    (util::BinaryBlockFile& destFile, std::vector<T> const& data) const;
  
  
    private:
  
  /// Library settings and derivative information.
  struct HeaderInfo_t {
    
    HeaderSettings_t header;
    
    std::fstream::pos_type dataOffset
      = std::numeric_limits<std::fstream::pos_type>::max();
    
  }; // HeaderInfo_t
  
  
  std::filesystem::path const fLibraryPath; ///< Path to the file.
  
  std::optional<HeaderInfo_t> fHeader;
  
  /// Creates an object to open an existing library file.
  util::BinaryBlockFile openLibraryFile
    (std::ios::openmode mode = std::ios::binary | std::ios::in) const;
  
  /// Creates an object to open an existing library file.
  util::BinaryBlockFile writeLibraryFile
    (std::ios::openmode mode = std::ios::binary | std::ios::out | std::ios::app)
    const;
  
  /// Performs checks and default value assignment for the header.
  void fixHeader();
  
}; // phot::PhotonLibraryBinaryFileFormat


namespace phot {
  
  std::ostream& operator<< (
    std::ostream& out,
    phot::PhotonLibraryBinaryFileFormat::HeaderSettings_t const& header
    );
  
} // namespace phot



// -----------------------------------------------------------------------------
// --- inline implementation
// -----------------------------------------------------------------------------
inline phot::PhotonLibraryBinaryFileFormat::PhotonLibraryBinaryFileFormat
  (std::filesystem::path const& libraryPath)
  : fLibraryPath(libraryPath)
{}


// -----------------------------------------------------------------------------
// --- template implementation
// -----------------------------------------------------------------------------
template <typename T>
void phot::PhotonLibraryBinaryFileFormat::writeData
  (std::vector<T> const& data) const
{
  
  util::BinaryBlockFile destFile = writeLibraryFile(std::ios::app);
  
  writeData(destFile, data);
  
} // phot::PhotonLibraryBinaryFileFormat::writeData()


// -----------------------------------------------------------------------------
template <typename T>
void phot::PhotonLibraryBinaryFileFormat::writeFile
  (std::vector<T> const& data) const
{
  
  util::BinaryBlockFile destFile = writeLibraryFile(std::ios::trunc);
  
  try {
    writeFile(destFile, data); 
  }
  catch (std::exception const& e) {
    throw util::encapsulateStdException("PhotonLibraryBinaryFileFormat", e)
      << "phot::PhotonLibraryBinaryFileFormat::writeFile(): "
      "error writing photon library file '" << fLibraryPath
      << "' (see encapsulated exceptions for the details)\n"
      ;
  }
  
} // phot::PhotonLibraryBinaryFileFormat::writeFile()


// -----------------------------------------------------------------------------
template <typename T>
void phot::PhotonLibraryBinaryFileFormat::writeData
  (util::BinaryBlockFile& destFile, std::vector<T> const& data) const
{
  
  // there needs to be a tiny bit more care otherwise (add padding after data)
  // and I don't want to bother with this now;
  // `FileBlockInfo` can provide the needed information on how much to pad
  // (see implementation of `FileBlock::writePayload()`)
  static_assert(util::fblock::alignsWithWord<T>(),
    "Only data types with size multiple of util::FileBlockWordSize supported."
    );
  
  try {
    destFile.writeBlockAndPayload
      ({ "PHVS", data.size() * sizeof(T) }, data.data());
  }
  catch (std::exception const& e) {
    throw util::encapsulateStdException("PhotonLibraryBinaryFileFormat", e)
      << "phot::PhotonLibraryBinaryFileFormat::writeData(): error writing "
      << data.size() << " entries of visibility data\n";
  }
  
} // phot::PhotonLibraryBinaryFileFormat::writeData()


// -----------------------------------------------------------------------------
template <typename T>
void phot::PhotonLibraryBinaryFileFormat::writeFile
  (util::BinaryBlockFile& destFile, std::vector<T> const& data) const
{
  
  writeHeader(destFile);
  writeData(destFile, data);
  writeFooter(destFile);
  
} // phot::PhotonLibraryBinaryFileFormat::writeFile()


// -----------------------------------------------------------------------------


#endif // LARSIM_PHOTONPROPAGATION_PHOTONLIBRARYBINARYFILEFORMAT_H
