#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art_root_io/TFileService.h"

#include "larsim/PhotonPropagation/PhotonLibraryBinaryFileFormat.h"
#include "larsim/PhotonPropagation/PhotonLibrary.h"
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

  std::string const PhotonLibrary::OpChannelBranchName = "OpChannel";

  //------------------------------------------------------------

  void PhotonLibrary::StoreLibraryToFile(std::string LibraryFile, bool storeReflected, bool storeReflT0, size_t storeTiming) const
  {
    mf::LogInfo("PhotonLibrary") << "Writing photon library to input file: " << LibraryFile.c_str()<<std::endl;

    art::ServiceHandle<art::TFileService const> tfs;

    TTree *tt = tfs->make<TTree>("PhotonLibraryData","PhotonLibraryData");


    Int_t     Voxel          = 0;
    Int_t     OpChannel      = 0;
    Float_t   Visibility     = 0;
    Float_t   ReflVisibility = 0;
    Float_t   ReflTfirst     = 0;
    Float_t   *timing_par = nullptr;

    tt->Branch("Voxel",      &Voxel,      "Voxel/I");
    tt->Branch(OpChannelBranchName.c_str(),  &OpChannel,  (OpChannelBranchName + "/I").c_str());
    tt->Branch("Visibility", &Visibility, "Visibility/F");

    if(storeTiming!=0)
    {
      if (!hasTiming()) {
        // if this happens, you need to call CreateEmptyLibrary() with storeReflected set true
        throw cet::exception("PhotonLibrary")
          << "StoreLibraryToFile() requested to store the time propagation distribution parameters, which was not simulated.";
      }
      tt->Branch("timing_par", timing_par, Form("timing_par[%i]/F",size_t2int(storeTiming)));
       if (fLookupTable.size() != fTimingParLookupTable.size())
        throw cet::exception(" Photon Library ") << "Time propagation lookup table is different size than Direct table \n"
                                                   << "this should not be happening. ";
    }

    if(storeReflected)
    {
      if (!hasReflected()) {
        // if this happens, you need to call CreateEmptyLibrary() with storeReflected set true
        throw cet::exception("PhotonLibrary")
          << "StoreLibraryToFile() requested to store reflected light, which was not simulated.";
      }
      tt->Branch("ReflVisibility", &ReflVisibility, "ReflVisibility/F");
      if (fLookupTable.size() != fReflLookupTable.size())
          throw cet::exception(" Photon Library ") << "Reflected light lookup table is different size than Direct table \n"
                                                   << "this should not be happening. ";
    }
    if(storeReflT0) {
      if (!hasReflectedT0()) {
        // if this happens, you need to call CreateEmptyLibrary() with storeReflectedT0 set true
        throw cet::exception("PhotonLibrary")
          << "StoreLibraryToFile() requested to store reflected light timing, which was not simulated.";
      }
      tt->Branch("ReflTfirst", &ReflTfirst, "ReflTfirst/F");
    }
    for(size_t ivox=0; ivox!= fNVoxels; ++ivox)
    {
      for(size_t ichan=0; ichan!= fNOpChannels; ++ichan)
      {
        Visibility = uncheckedAccess(ivox, ichan);
        if(storeReflected)
          ReflVisibility = uncheckedAccessRefl(ivox, ichan);
        if(storeReflT0)
          ReflTfirst = uncheckedAccessReflT(ivox, ichan);
	if(storeTiming!=0)
	{
	  for (size_t i =0; i<storeTiming; i++) timing_par[i] = uncheckedAccessTimingPar(ivox, ichan, i);

	}
        if (Visibility > 0 || ReflVisibility > 0)
        {
          Voxel      = ivox;
          OpChannel  = ichan;
          // visibility(ies) is(are) already set
          tt->Fill();
        }
      }
    }
  }


  //------------------------------------------------------------
  void PhotonLibrary::StoreLibraryToPlainDataFiles(
    std::string const& directPath, std::string const& reflectedPath,
    sim::PhotonVoxelDef const& voxelDefs,
    std::string configuration /* = "" */
    ) const
  {
  
    if (!directPath.empty()) {
      mf::LogVerbatim("PhotonLibrary")
        << "Saving the direct light visibility library information as '"
        << directPath << "'.";
      StoreLibraryToPlainDataFile
        (directPath, voxelDefs, fLookupTable, configuration);
    }
    
    if (!reflectedPath.empty()) {
      if (!hasReflected()) {
        throw cet::exception("PhotonLibrary")
          << "Requested the serialization into binary file '" << reflectedPath
          << "' for reflected light, which is not included in the library.\n";
      }
      mf::LogVerbatim("PhotonLibrary")
        << "Saving the reflected light visibility library information as '"
        << reflectedPath << "'.";
      StoreLibraryToPlainDataFile
        (reflectedPath, voxelDefs, fReflLookupTable, configuration);
    }
    
  } // PhotonLibrary::StoreLibraryToPlainDataFiles()
  
  
  //------------------------------------------------------------

  void PhotonLibrary::CreateEmptyLibrary(
    size_t NVoxels, size_t NOpChannels,
    bool storeReflected /* = false */,
    bool storeReflT0 /* = false */,
    size_t storeTiming /* = false */
  ) {
    fLookupTable.clear();
    fReflLookupTable.clear();
    fReflTLookupTable.clear();
    fTimingParLookupTable.clear();
    fTimingParTF1LookupTable.clear();

    fNVoxels     = NVoxels;
    fNOpChannels = NOpChannels;

    fLookupTable.resize(LibrarySize());
    fHasReflected = storeReflected;
    if (storeReflected) fReflLookupTable.resize(LibrarySize());
    fHasReflectedT0 = storeReflT0;
    if (storeReflT0) fReflTLookupTable.resize(LibrarySize());
    fHasTiming = storeTiming;
    if (storeTiming!=0)
    {
      fTimingParLookupTable.resize(LibrarySize());
      fTimingParTF1LookupTable.resize(LibrarySize());
    }
  }


  //------------------------------------------------------------

  void PhotonLibrary::LoadLibraryFromFile(std::string LibraryFile, size_t NVoxels, bool getReflected, bool getReflT0, size_t getTiming, int fTimingMaxRange)
  {
    fLookupTable.clear();
    fReflLookupTable.clear();
    fReflTLookupTable.clear();
    fTimingParLookupTable.clear();
    fTimingParTF1LookupTable.clear();

    mf::LogInfo("PhotonLibrary") << "Reading photon library from input file: " << LibraryFile.c_str()<<std::endl;

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
                mf::LogError("PhotonLibrary") << "PhotonLibraryData not found in file" <<LibraryFile;
            }
        }
      }
    catch(...)
      {
        throw cet::exception("PhotonLibrary") << "Error in ttree load, reading photon library: " << LibraryFile << "\n";
      }


    Int_t     Voxel;
    Int_t     OpChannel;
    Float_t   Visibility;
    Float_t   ReflVisibility;
    Float_t   ReflTfirst;
    std::vector<Float_t>   timing_par;

    tt->SetBranchAddress("Voxel",      &Voxel);
    tt->SetBranchAddress("OpChannel",  &OpChannel);
    tt->SetBranchAddress("Visibility", &Visibility);

    fHasTiming = getTiming;

    fHasReflected = getReflected;
    if(getReflected)
      tt->SetBranchAddress("ReflVisibility", &ReflVisibility);
    fHasReflectedT0 = getReflT0;
    if(getReflT0)
      tt->SetBranchAddress("ReflTfirst", &ReflTfirst);

    fNVoxels     = NVoxels;
    fNOpChannels = PhotonLibrary::ExtractNOpChannels(tt); // EXPENSIVE!!!

    // with STL vectors, where `resize()` directly controls the allocation of
    // memory, reserving the space is redundant; not so with `util::LazyVector`,
    // where `resize()` never increases the memory; `data_init()` allocates
    // all the storage we need at once, effectively suppressing the laziness
    // of the vector (by design, that was only relevant in `CreateEmptyLibrary()`)
    fLookupTable.resize(LibrarySize());
    fLookupTable.data_init(LibrarySize());
    
    if(fHasTiming!=0)
    {
      timing_par.resize(getTiming);
      tt->SetBranchAddress("timing_par", timing_par.data());
      fTimingParNParameters=fHasTiming;
      TNamed *n = (TNamed*)f->Get("fTimingParFormula");
      if(!n) mf::LogError("PhotonLibrary") <<"Error reading the photon propagation formula. Please check the photon library." << std::endl;
      fTimingParFormula = n->GetTitle();
      fTimingParTF1LookupTable.resize(LibrarySize());
      fTimingParTF1LookupTable.data_init(LibrarySize());
      mf::LogInfo("PhotonLibrary") <<"Time parametrization is activated. Using the formula: "<<  fTimingParFormula << " with " << fTimingParNParameters << " parameters."<< std::endl;
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

      // Set the visibility at this optical channel
      uncheckedAccess(Voxel, OpChannel) = Visibility;

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

    mf::LogInfo("PhotonLibrary") <<"Photon lookup table size : "<<  NVoxels << " voxels,  " << fNOpChannels<<" channels";

    try
      {
	f->Close();
      }
    catch(...)
      {
	mf::LogError("PhotonLibrary") << "Error in closing file : " << LibraryFile;
      }

  }

  //----------------------------------------------------

  float PhotonLibrary::GetCount(size_t Voxel, size_t OpChannel) const
  {
    if ((Voxel >= fNVoxels) || (OpChannel >= fNOpChannels))
      return 0;
    else
      return uncheckedAccess(Voxel, OpChannel);
  }
  //----------------------------------------------------

  float PhotonLibrary::GetTimingPar(size_t Voxel, size_t OpChannel, size_t parnum) const
  {
    if ((Voxel >= fNVoxels) || (OpChannel >= fNOpChannels))
      return 0;
    else
      return uncheckedAccessTimingPar(Voxel, OpChannel, parnum);
  }  //----------------------------------------------------

  float PhotonLibrary::GetReflCount(size_t Voxel, size_t OpChannel) const
  {
    if ((Voxel >= fNVoxels) || (OpChannel >= fNOpChannels))
      return 0;
    else
      return uncheckedAccessRefl(Voxel, OpChannel);
  }
  //----------------------------------------------------

  float PhotonLibrary::GetReflT0(size_t Voxel, size_t OpChannel) const
  {
    if ((Voxel >= fNVoxels) || (OpChannel >= fNOpChannels))
      return 0;
    else
      return uncheckedAccessReflT(Voxel, OpChannel);
  }

  //----------------------------------------------------

  void PhotonLibrary::SetCount(size_t Voxel, size_t OpChannel, float Count)
  {
    if ((Voxel >= fNVoxels) || (OpChannel >= fNOpChannels))
      mf::LogError("PhotonLibrary")<<"Error - attempting to set count in voxel " << Voxel<<" which is out of range";
    else
      uncheckedAccess(Voxel, OpChannel) = Count;
  }
  //----------------------------------------------------

  void PhotonLibrary::SetTimingPar(size_t Voxel, size_t OpChannel, float Count, size_t parnum)
  {
    if ((Voxel >= fNVoxels) || (OpChannel >= fNOpChannels))
      mf::LogError("PhotonLibrary")<<"Error - attempting to set timing t0 count in voxel " << Voxel<<" which is out of range";
    else
      uncheckedAccessTimingPar(Voxel, OpChannel, parnum) = Count;
  }
  //----------------------------------------------------

  void PhotonLibrary::SetTimingTF1(size_t Voxel, size_t OpChannel, TF1 func)
  {
    if ((Voxel >= fNVoxels) || (OpChannel >= fNOpChannels))
      mf::LogError("PhotonLibrary")<<"Error - attempting to set a propagation function in voxel " << Voxel<<" which is out of range";
    else
      uncheckedAccessTimingTF1(Voxel, OpChannel) = func;
  }
  //----------------------------------------------------


  void PhotonLibrary::SetReflCount(size_t Voxel, size_t OpChannel, float Count)
  {
    if ((Voxel >= fNVoxels) || (OpChannel >= fNOpChannels))
      mf::LogError("PhotonLibrary")<<"Error - attempting to set count in voxel " << Voxel<<" which is out of range";
    else
      uncheckedAccessRefl(Voxel, OpChannel) = Count;
  }
  //----------------------------------------------------

  void PhotonLibrary::SetReflT0(size_t Voxel, size_t OpChannel, float Count)
  {
    if ((Voxel >= fNVoxels) || (OpChannel >= fNOpChannels))
      mf::LogError("PhotonLibrary")<<"Error - attempting to set count in voxel " << Voxel<<" which is out of range";
    else
      uncheckedAccessReflT(Voxel, OpChannel) = Count;
  }

  //----------------------------------------------------

  auto PhotonLibrary::GetCounts(size_t Voxel) const -> Counts_t
  {
    return { (Voxel < fNVoxels)? fLookupTable.data_address(uncheckedIndex(Voxel, 0)): nullptr };
  }

  //----------------------------------------------------

  const std::vector<float>* PhotonLibrary::GetTimingPars(size_t Voxel) const
  {
    if (Voxel >= fNVoxels) return nullptr;
    else return fTimingParLookupTable.data_address(uncheckedIndex(Voxel, 0));
  }

  //----------------------------------------------------

  TF1* PhotonLibrary::GetTimingTF1s(size_t Voxel) const
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
     * TF1 const* PhotonLibrary::GetTimingTF1s(size_t Voxel) const
     *
     * and the users should update their code accordingly.
     */
    else return const_cast<TF1*>(fTimingParTF1LookupTable.data_address(uncheckedIndex(Voxel, 0)));
  }

  //----------------------------------------------------

  auto PhotonLibrary::GetReflCounts(size_t Voxel) const -> Counts_t
  {
    if (Voxel >= fNVoxels) return { nullptr };
    else return { fReflLookupTable.data_address(uncheckedIndex(Voxel, 0)) };
  }

  //----------------------------------------------------

  auto PhotonLibrary::GetReflT0s(size_t Voxel) const -> T0s_t
  {
    if (Voxel >= fNVoxels) return nullptr;
    else return fReflTLookupTable.data_address(uncheckedIndex(Voxel, 0));
  }

  //------------------------------------------------------------
  void PhotonLibrary::StoreLibraryToPlainDataFile(
    std::string const& outputFilePath,
    sim::PhotonVoxelDef const& voxelDefs,
    util::LazyVector<float> const& table,
    std::string configuration /* = "" */
  ) const {
    
    phot::PhotonLibraryBinaryFileFormat outFile(outputFilePath);
    
    auto const nEntries = fNOpChannels * fNVoxels;
    
    auto const lowerPoint = voxelDefs.GetRegionLowerCorner();
    auto const upperPoint = voxelDefs.GetRegionUpperCorner();
    auto const voxelSizes = voxelDefs.GetVoxelSize();
    auto const steps = voxelDefs.GetSteps();
    assert(fNVoxels == voxelDefs.GetNVoxels());
    
    phot::PhotonLibraryBinaryFileFormat::HeaderSettings_t header;
    header.version = phot::PhotonLibraryBinaryFileFormat::LatestFormatVersion;
    header.configuration = std::move(configuration);
    header.nEntries = nEntries;
    header.nChannels = fNOpChannels;
    header.nVoxels = fNVoxels;
    header.axes[0] = { steps[0], lowerPoint.X(), upperPoint.X(), voxelSizes.X() };
    header.axes[1] = { steps[1], lowerPoint.Y(), upperPoint.Y(), voxelSizes.Y() };
    header.axes[2] = { steps[2], lowerPoint.Z(), upperPoint.Z(), voxelSizes.Z() };
    outFile.setHeader(std::move(header));
    std::ostringstream sstr;
    outFile.dumpInfo(sstr);
    mf::LogInfo("PhotonLibrary")
      << "Writing library to '" << outputFilePath << "':\n" << sstr.str();
    
    try {
      //
      // ideally we could use the data buffer directly; but if it is lazy and
      // missing data, well, we can't any more; so we borrow a lot of memory...
      //
      std::vector<float> visData(nEntries);
      auto itDest = visData.begin();
      for (auto i: util::counter<std::size_t>(nEntries)) *itDest++ = table[i];
      assert(itDest == visData.end());
      outFile.writeFile(visData);
    }
    catch (cet::exception const& e) {
      throw cet::exception("PhotonLibrary", "", e)
        << "PhotonLibrary::StoreLibraryToPlainDataFile():"
        " error while writing library into '"
        << outputFilePath << "'.\n";
    }
    
  } // PhotonLibrary::StoreLibraryToPlainDataFile()
  
  
  //----------------------------------------------------

  size_t PhotonLibrary::ExtractNOpChannels(TTree* tree) {
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

    MF_LOG_DEBUG("PhotonLibrary")
      << "Detected highest channel to be " << maxChannel << " from " << iEntry
      << " tree entries";

    // restore the old branch address
    channelBranch->SetAddress(oldAddress);

    return size_t(maxChannel + 1);

  } // PhotonLibrary::ExtractNOpChannels()


  //----------------------------------------------------

}
