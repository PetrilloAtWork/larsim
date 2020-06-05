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


#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art_root_io/TFileService.h"

#include "larcorealg/CoreUtils/counter.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "RtypesCore.h"
#include "TFile.h"
#include "TTree.h"
#include "TKey.h"
#include "TF1.h"
#include "TBranch.h"
#include "TNamed.h"
#include "TString.h"

#include <filesystem>

namespace phot{

  std::string const BinaryFilePhotonLibrary::OpChannelBranchName = "OpChannel";

  //------------------------------------------------------------

  void BinaryFilePhotonLibrary::LoadLibraryFromFile(
    std::string const& LibraryFile,
    std::string const& DirectVisibilityPlainFilePath,
    unsigned int nVoxels, unsigned int nOpChannels,
    bool getReflected, bool getReflT0, size_t getTiming, int fTimingMaxRange
    )
  {
    
    LoadLibraryFromPlainDataFile
      (DirectVisibilityPlainFilePath, nVoxels, nOpChannels);
    
    fReflLookupTable.clear();
    fReflTLookupTable.clear();
    fTimingParLookupTable.clear();
    fTimingParTF1LookupTable.clear();

    if (!getReflected && !getReflT0 && !getTiming) return;
    
    mf::LogInfo("BinaryFilePhotonLibrary") << "Reading photon library from input file: " << LibraryFile.c_str()<<std::endl;

    TFile *f = nullptr;
    TTree *tt = nullptr;

    try
      {
	f  =  TFile::Open(LibraryFile.c_str());
	tt =  (TTree*)f->Get("PhotonLibraryData");
        if (!tt) { // Library not in the top directory
            TKey *key = f->FindKeyAny("PhotonLibraryData");
            if (key)
                tt = (TTree*)key->ReadObj();
            else {
                mf::LogError("BinaryFilePhotonLibrary") << "PhotonLibraryData not found in file" <<LibraryFile;
            }
        }
      }
    catch(...)
      {
        throw cet::exception("BinaryFilePhotonLibrary") << "Error in ttree load, reading photon library: " << LibraryFile << "\n";
      }


    Int_t     Voxel;
    Int_t     OpChannel;
    Float_t   ReflVisibility;
    Float_t   ReflTfirst;
    std::vector<Float_t>   timing_par;

    tt->SetBranchAddress("Voxel",      &Voxel);
    tt->SetBranchAddress("OpChannel",  &OpChannel);

    fHasTiming = getTiming;

    fHasReflected = getReflected;
    if(getReflected)
      tt->SetBranchAddress("ReflVisibility", &ReflVisibility);
    fHasReflectedT0 = getReflT0;
    if(getReflT0)
      tt->SetBranchAddress("ReflTfirst", &ReflTfirst);

    fNVoxels     = nVoxels;
    fNOpChannels = BinaryFilePhotonLibrary::ExtractNOpChannels(tt); // EXPENSIVE!!!

    // with STL vectors, where `resize()` directly controls the allocation of
    // memory, reserving the space is redundant; not so with `util::LazyVector`,
    // where `resize()` never increases the memory; `data_init()` allocates
    // all the storage we need at once, effectively suppressing the laziness
    // of the vector (by design, that was only relevant in `CreateEmptyLibrary()`)
    
    if(fHasTiming!=0)
    {
      timing_par.resize(getTiming);
      tt->SetBranchAddress("timing_par", timing_par.data());
      fTimingParNParameters=fHasTiming;
      TNamed *n = (TNamed*)f->Get("fTimingParFormula");
      if(!n) mf::LogError("BinaryFilePhotonLibrary") <<"Error reading the photon propagation formula. Please check the photon library." << std::endl;
      fTimingParFormula = n->GetTitle();
      fTimingParTF1LookupTable.resize(LibrarySize());
      fTimingParTF1LookupTable.data_init(LibrarySize());
      mf::LogInfo("BinaryFilePhotonLibrary") <<"Time parametrization is activated. Using the formula: "<<  fTimingParFormula << " with " << fTimingParNParameters << " parameters."<< std::endl;
    }
    if(fHasReflected) {
      fReflLookupTable.resize(LibrarySize());
      fReflLookupTable.data_init(LibrarySize());
    }
    if(fHasReflectedT0) {
      fReflTLookupTable.resize(LibrarySize());
      fReflTLookupTable.data_init(LibrarySize());
    }

    size_t NEntries = tt->GetEntries();

    for(size_t i=0; i!=NEntries; ++i) {


      tt->GetEntry(i);

      if(fHasReflected)
	uncheckedAccessRefl(Voxel, OpChannel) = ReflVisibility;
      if(fHasReflectedT0)
	uncheckedAccessReflT(Voxel, OpChannel) = ReflTfirst;
      if(fHasTiming!=0)
      {
        // TODO: use TF1::Copy
	TF1 timingfunction(Form("timing_%i_%i",Voxel,OpChannel),fTimingParFormula.c_str(),timing_par[0],fTimingMaxRange);
        timingfunction.SetParameter(0,0.0);//first parameter is now in the range. Let's do this to keep compatible with old libraries.
	for (size_t k=1;k<fTimingParNParameters;k++)
        {
	  timingfunction.SetParameter(k,timing_par[k]);
        }

	uncheckedAccessTimingTF1(Voxel,OpChannel) = timingfunction;
      }
    } // for entries

    mf::LogInfo("BinaryFilePhotonLibrary") <<"Photon lookup table size : "<<  nVoxels << " voxels,  " << fNOpChannels<<" channels";

    try
      {
	f->Close();
      }
    catch(...)
      {
	mf::LogError("BinaryFilePhotonLibrary") << "Error in closing file : " << LibraryFile;
      }

  }

  //------------------------------------------------------------
  void BinaryFilePhotonLibrary::LoadLibraryFromPlainDataFile
    (std::string const& fileName, unsigned int nVoxels, unsigned int nOpChannels)
  {
    
    // bleargh... this is because we want a delayed initialization
    // without paying the (small) price of a std::optional or likewise.
    fLookupTableFile = LookupTableFile_t{ fileName, nVoxels, nOpChannels };
    
    fNVoxels = nVoxels;
    fNOpChannels = nOpChannels;
    
    mf::LogVerbatim("BinaryFilePhotonLibrary")
      << "BinaryFilePhotonLibrary: loaded VUV light map from '" << fileName
      << "' (" << fNVoxels << " voxels, " << fNOpChannels << " channels)";
    
  } // BinaryFilePhotonLibrary::LoadLibraryFromPlainDataFile()
  
  
  //----------------------------------------------------
  float BinaryFilePhotonLibrary::GetCount(size_t Voxel, size_t OpChannel) const
  {
    
    if ((Voxel >= fNVoxels) || (OpChannel >= fNOpChannels))
      return 0;
    else
      return fLookupTableFile.getDataAt(Voxel, OpChannel);
  }
  
  
  //----------------------------------------------------

  float BinaryFilePhotonLibrary::GetTimingPar(size_t Voxel, size_t OpChannel, size_t parnum) const
  {
    if ((Voxel >= fNVoxels) || (OpChannel >= fNOpChannels))
      return 0;
    else
      return uncheckedAccessTimingPar(Voxel, OpChannel, parnum);
  }  //----------------------------------------------------

  float BinaryFilePhotonLibrary::GetReflCount(size_t Voxel, size_t OpChannel) const
  {
    if ((Voxel >= fNVoxels) || (OpChannel >= fNOpChannels))
      return 0;
    else
      return uncheckedAccessRefl(Voxel, OpChannel);
  }
  //----------------------------------------------------

  float BinaryFilePhotonLibrary::GetReflT0(size_t Voxel, size_t OpChannel) const
  {
    if ((Voxel >= fNVoxels) || (OpChannel >= fNOpChannels))
      return 0;
    else
      return uncheckedAccessReflT(Voxel, OpChannel);
  }

  //----------------------------------------------------

  void BinaryFilePhotonLibrary::SetTimingPar(size_t Voxel, size_t OpChannel, float Count, size_t parnum)
  {
    if ((Voxel >= fNVoxels) || (OpChannel >= fNOpChannels))
      mf::LogError("BinaryFilePhotonLibrary")<<"Error - attempting to set timing t0 count in voxel " << Voxel<<" which is out of range";
    else
      uncheckedAccessTimingPar(Voxel, OpChannel, parnum) = Count;
  }
  //----------------------------------------------------

  void BinaryFilePhotonLibrary::SetTimingTF1(size_t Voxel, size_t OpChannel, TF1 func)
  {
    if ((Voxel >= fNVoxels) || (OpChannel >= fNOpChannels))
      mf::LogError("BinaryFilePhotonLibrary")<<"Error - attempting to set a propagation function in voxel " << Voxel<<" which is out of range";
    else
      uncheckedAccessTimingTF1(Voxel, OpChannel) = func;
  }
  //----------------------------------------------------


  void BinaryFilePhotonLibrary::SetReflCount(size_t Voxel, size_t OpChannel, float Count)
  {
    if ((Voxel >= fNVoxels) || (OpChannel >= fNOpChannels))
      mf::LogError("BinaryFilePhotonLibrary")<<"Error - attempting to set count in voxel " << Voxel<<" which is out of range";
    else
      uncheckedAccessRefl(Voxel, OpChannel) = Count;
  }
  //----------------------------------------------------

  void BinaryFilePhotonLibrary::SetReflT0(size_t Voxel, size_t OpChannel, float Count)
  {
    if ((Voxel >= fNVoxels) || (OpChannel >= fNOpChannels))
      mf::LogError("BinaryFilePhotonLibrary")<<"Error - attempting to set count in voxel " << Voxel<<" which is out of range";
    else
      uncheckedAccessReflT(Voxel, OpChannel) = Count;
  }

  //----------------------------------------------------

  auto BinaryFilePhotonLibrary::GetCounts(size_t Voxel) const -> Counts_t
  {
    if (Voxel >= fNVoxels) return nullptr;
    return { fLookupTableFile.getDataAt(Voxel) }; // pointer owns the data
  }

  //----------------------------------------------------

  const std::vector<float>* BinaryFilePhotonLibrary::GetTimingPars(size_t Voxel) const
  {
    if (Voxel >= fNVoxels) return nullptr;
    else return fTimingParLookupTable.data_address(uncheckedIndex(Voxel, 0));
  }

  //----------------------------------------------------

  TF1* BinaryFilePhotonLibrary::GetTimingTF1s(size_t Voxel) const
  {
    if (Voxel >= fNVoxels) return nullptr;
    /*
     * Sorry, Universe, but we can't undergo ROOT's bad design hell.
     * TF1::GetRandom() is non-const member, because it uses some internal
     * integral information which can be produced on the spot instead than
     * always be present. That's called caching, it's Good, but it must not
     * interfere with constantness of the interface (in fact, this is one of
     * the few acceptable uses of `mutable` members).
     * Because of this, this method can't return a constant `TF1`, therefore
     * it can't be constant, therefore the underlying access returning a
     * constant object is not acceptable.
     * So I do the Bad thing.
     * Plus I opened JIRA ROOT-9549
     * (https://sft.its.cern.ch/jira/browse/ROOT-9549).
     * After that is solved, this method should become:
     *
     * TF1 const* BinaryFilePhotonLibrary::GetTimingTF1s(size_t Voxel) const
     *
     * and the users should update their code accordingly.
     */
    else return const_cast<TF1*>(fTimingParTF1LookupTable.data_address(uncheckedIndex(Voxel, 0)));
  }

  //----------------------------------------------------

  auto BinaryFilePhotonLibrary::GetReflCounts(size_t Voxel) const -> Counts_t
  {
    if (Voxel >= fNVoxels) return { nullptr };
    else return { fReflLookupTable.data_address(uncheckedIndex(Voxel, 0)) };
  }

  //----------------------------------------------------

  auto BinaryFilePhotonLibrary::GetReflT0s(size_t Voxel) const -> T0s_t
  {
    if (Voxel >= fNVoxels) return nullptr;
    else return fReflTLookupTable.data_address(uncheckedIndex(Voxel, 0));
  }

  //----------------------------------------------------
  std::size_t BinaryFilePhotonLibrary::LookupTableSize() const {
    return fLookupTableFile.NData();
  } // BinaryFilePhotonLibrary::LookupTableSize()
  
  
  //----------------------------------------------------

  size_t BinaryFilePhotonLibrary::ExtractNOpChannels(TTree* tree) {
    TBranch* channelBranch = tree->GetBranch(OpChannelBranchName.c_str());
    if (!channelBranch) {
      throw art::Exception(art::errors::NotFound)
        << "Tree '" << tree->GetName() << "' has no branch 'OpChannel'";
    }

    // fix a new local address for the branch
    char* oldAddress = channelBranch->GetAddress();
    Int_t channel;
    channelBranch->SetAddress(&channel);
    Int_t maxChannel = -1;

    // read all the channel values and pick the largest one
    Long64_t iEntry = 0;
    while (channelBranch->GetEntry(iEntry++)) {
      if (channel > maxChannel) maxChannel = channel;
    } // while

    MF_LOG_DEBUG("BinaryFilePhotonLibrary")
      << "Detected highest channel to be " << maxChannel << " from " << iEntry
      << " tree entries";

    // restore the old branch address
    channelBranch->SetAddress(oldAddress);

    return size_t(maxChannel + 1);

  } // BinaryFilePhotonLibrary::ExtractNOpChannels()


  //----------------------------------------------------

}
