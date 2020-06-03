// ////////////////////////////////////////////////////////////////////////
//
//  \file PhotonVisibilityService_service.cc
//
////////////////////////////////////////////////////////////////////////
//
//  Ben Jones, MIT 2012
//
//  This service reports the visibility of a particular point in
//  the detector to each OpDet.  This is used by the fast
//  optical simulation and by track-light association algorithms.
//
//  Visibility is defined as the fraction of isotropically produced
//  photons from a detector voxel which are expected to reach the
//  OpDet in question.
//
//  This information is lookup up from a previously generated
//  optical library file, whose path is specified to this service.
//
//  Note that it is important that the voxelization schemes match
//  between the library and the service instance for sensible results.
//
//

// LArSoft includes
#include "larsim/PhotonPropagation/PhotonVisibilityService.h"
#include "larsim/PhotonPropagation/PhotonLibrary.h"
#include "larsim/Simulation/PhotonVoxels.h"
#include "larcore/Geometry/Geometry.h"
#include "larcorealg/Geometry/OpDetGeo.h"

#include "larsim/PhotonPropagation/PhotonLibraryHybrid.h"

// framework libraries
#include "art/Utilities/make_tool.h"
#include "cetlib_except/exception.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

// ROOT libraries
#include "TF1.h"

// C/C++ standard libraries
#include <optional>


namespace phot{

  PhotonVisibilityService::~PhotonVisibilityService()
  {
    delete fparslogNorm;
    delete fparslogNorm_far;
    delete fparsMPV;
    delete fparsMPV_far;
    delete fparsWidth;
    delete fparsCte;
    delete fparsCte_far;
    delete fparsSlope;
    delete fparslogNorm_refl;
    delete fparsMPV_refl;
    delete fparsWidth_refl;
    delete fparsCte_refl;
    delete fparsSlope_refl;
    delete fTheLibrary;
  }

  //--------------------------------------------------------------------
  PhotonVisibilityService::PhotonVisibilityService(fhicl::ParameterSet const& pset) :

    fCurrentVoxel(0),
    fCurrentValue(0.),
    fXmin(0.),
    fXmax(0.),
    fYmin(0.),
    fYmax(0.),
    fZmin(0.),
    fZmax(0.),
    fNx(0),
    fNy(0),
    fNz(0),
    fUseCryoBoundary(false),
    fLibraryBuildJob(false),
    fDoNotLoadLibrary(false),
    fParameterization(false),
    fHybrid(false),
    fStoreReflected(false),
    fStoreReflT0(false),
    fIncludePropTime(false),
    fUseNhitsModel(false),
    fParPropTime(false),
    fParPropTime_npar(0),
    fParPropTime_formula(),
    fParPropTime_MaxRange(),
    fInterpolate(false),
    fReflectOverZeroX(false),
    fparslogNorm(nullptr),
    fparslogNorm_far(nullptr),
    fparsMPV(nullptr),
    fparsMPV_far(nullptr),
    fparsWidth(nullptr),
    fparsCte(nullptr),
    fparsCte_far(nullptr),
    fparsSlope(nullptr),
    fD_break(0.0),
    fD_max(0.0),
    fTF1_sampling_factor(0.0),
    fparslogNorm_refl(nullptr),
    fparsMPV_refl(nullptr),
    fparsWidth_refl(nullptr),
    fparsCte_refl(nullptr),
    fparsSlope_refl(nullptr),
    fT0_max(0.0),
    fT0_break_point(0.0),
    fLibraryFile(),
    fTheLibrary(nullptr),
    fVoxelDef()
  {
    this->reconfigure(pset);

    if (pset.has_key("ReflectOverZeroX")) { // legacy parameter warning
      if (pset.has_key("Mapping")) {
        throw art::Exception(art::errors::Configuration) <<
          "`PhotonVisbilityService` configuration specifies both `Mapping` and `ReflectOverZeroX`."
          " Please remove the latter (and use `PhotonMappingXMirrorTransformations` tool)."
          ;
      }
      else {
        mf::LogWarning("PhotonVisbilityService")
          << "Please update the configuration of `PhotonVisbilityService` service"
          " replacing `ReflectOverZeroX` with tool configuration:"
          "\n  Mapping: { tool_type: \"PhotonMappingXMirrorTransformations\" }"
          ;
      }
    } // if
    fhicl::ParameterSet mapDefaultSet;
    mapDefaultSet.put("tool_type",
      fReflectOverZeroX
        ? "PhotonMappingXMirrorTransformations"
        : "PhotonMappingIdentityTransformations"
      );
    fMapping = art::make_tool<phot::IPhotonMappingTransformations>
      (pset.get<fhicl::ParameterSet>("Mapping", mapDefaultSet));

    mf::LogInfo("PhotonVisibilityService")<<"PhotonVisbilityService initializing"<<std::endl;
  }

  //--------------------------------------------------------------------
  void PhotonVisibilityService::LoadLibrary() const
  {
    // Don't do anything if the library has already been loaded.

    if (fTheLibrary) return;

    if((!fLibraryBuildJob)&&(!fDoNotLoadLibrary)) {
      std::string LibraryFileWithPath;
      cet::search_path sp("FW_SEARCH_PATH");

      if( !sp.find_file(fLibraryFile, LibraryFileWithPath) )
        throw cet::exception("PhotonVisibilityService") << "Unable to find photon library in "  << sp.to_string() << "\n";

      if(fParameterization) {
        PhotonLibraryOnlyParametersCheck();
      }
      else {
        art::ServiceHandle<geo::Geometry const> geom;

        mf::LogInfo("PhotonVisibilityService") << "PhotonVisibilityService Loading photon library from file "
                                                << LibraryFileWithPath
                                                << " for "
                                                << GetVoxelDef().GetNVoxels()
                                                << " voxels and "
                                                << geom->NOpDets()
                                                << " optical detectors."
                                                << std::endl;

        if(fHybrid){
          PhotonLibraryOnlyParametersCheck();
          fTheLibrary = new PhotonLibraryHybrid(LibraryFileWithPath,
                                                GetVoxelDef());
        }
        else{
          PhotonLibrary* lib = new PhotonLibrary;
          
          fTheLibrary = lib;

          size_t NVoxels = GetVoxelDef().GetNVoxels();
          lib->LoadLibraryFromFile(LibraryFileWithPath, NVoxels, fStoreReflected, fStoreReflT0, fParPropTime_npar, fParPropTime_MaxRange);
          
          if (!fSaveAsBinaryFile.empty())
            lib->StoreLibraryToPlainDataFile(fSaveAsBinaryFile);
          
        }
      }
    }
    else {
      PhotonLibraryOnlyParametersCheck();
      art::ServiceHandle<geo::Geometry const> geom;

      size_t NOpDets = geom->NOpDets();
      size_t NVoxels = GetVoxelDef().GetNVoxels();
      mf::LogInfo("PhotonVisibilityService") << " Vis service running library build job.  Please ensure "
                                              << " job contains LightSource, LArG4, SimPhotonCounter"<<std::endl;
      PhotonLibrary* lib = new PhotonLibrary;
      fTheLibrary = lib;

      lib->CreateEmptyLibrary(NVoxels, NOpDets, fStoreReflected, fStoreReflT0, fParPropTime_npar);
    }

  } // PhotonVisibilityService::LoadLibrary()

  //--------------------------------------------------------------------
  void PhotonVisibilityService::StoreLibrary()
  {
    if(fTheLibrary == 0)
      LoadLibrary();

    if(fLibraryBuildJob )
      {

          if(fHybrid){
              std::cout<< "This is would be building a Hybrid Library. Not defined. "<<std::endl;
          }
          mf::LogInfo("PhotonVisibilityService") << " Vis service "
                                               << " Storing Library entries to file..." <<std::endl;
          PhotonLibrary* lib = dynamic_cast<PhotonLibrary*>(fTheLibrary);
          lib->StoreLibraryToFile(fLibraryFile, fStoreReflected, fStoreReflT0, fParPropTime_npar);
      }
  }


  //--------------------------------------------------------------------
  void PhotonVisibilityService::reconfigure(fhicl::ParameterSet const& p)
  {

    art::ServiceHandle<geo::Geometry const> geom;

    // Library details
    fLibraryBuildJob      = p.get< bool        >("LibraryBuildJob", false);
    fParameterization     = p.get< bool        >("DUNE10ktParameterization", false);
    fHybrid               = p.get< bool        >("HybridLibrary", false);
    fLibraryFile          = p.get< std::string >("LibraryFile", "");
    fDoNotLoadLibrary     = p.get< bool        >("DoNotLoadLibrary");
    fStoreReflected       = p.get< bool        >("StoreReflected", false);
    fStoreReflT0          = p.get< bool        >("StoreReflT0",  false);
    // Parametrizations (time and Nhits)
    fIncludePropTime      = p.get< bool        >("IncludePropTime", false);
    fUseNhitsModel        = p.get< bool        >("UseNhitsModel", false);
    fApplyVISBorderCorrection = p.get< bool    >("ApplyVISBorderCorrection", false);
    fVISBorderCorrectionType = p.get< std::string >("VIS_BORDER_correction_type","");
    fSaveAsBinaryFile     = p.get< std::string >("SaveAsBinaryFile", "");
    
    // Voxel parameters
    fUseCryoBoundary      = p.get< bool        >("UseCryoBoundary", false);
    fInterpolate          = p.get< bool        >("Interpolate", false);
    fReflectOverZeroX     = p.get< bool        >("ReflectOverZeroX", false);

    fParPropTime          = p.get< bool        >("ParametrisedTimePropagation", false);
    fParPropTime_npar     = p.get< size_t      >("ParametrisedTimePropagationNParameters", 0);
    fParPropTime_formula  = p.get< std::string >("ParametrisedTimePropagationFittedFormula","");
    fParPropTime_MaxRange = p.get< int         >("ParametrisedTimePropagationMaxRange", 200);

    if (!fParPropTime)
    {
      fParPropTime_npar=0;
    }

    if(!fUseNhitsModel) {

    if(fUseCryoBoundary)
      {
        double CryoBounds[6];
        geom->CryostatBoundaries(CryoBounds);
        fXmin = CryoBounds[0];
        fXmax = CryoBounds[1];
        fYmin = CryoBounds[2];
        fYmax = CryoBounds[3];
        fZmin = CryoBounds[4];
        fZmax = CryoBounds[5];
      }
    else
      {
        fXmin      = p.get< double       >("XMin"     );
        fXmax      = p.get< double       >("XMax"     );
        fYmin      = p.get< double       >("YMin"     );
        fYmax      = p.get< double       >("YMax"     );
        fZmin      = p.get< double       >("ZMin"     );
        fZmax      = p.get< double       >("ZMax"     );
      }

    fNx        = p.get< int          >("NX"       );
    fNy        = p.get< int          >("NY"       );
    fNz        = p.get< int          >("NZ"       );

    fVoxelDef = sim::PhotonVoxelDef(fXmin, fXmax, fNx, fYmin, fYmax, fNy, fZmin, fZmax, fNz);

    }

    if(fIncludePropTime)
      {

        // load VUV arrival time distribution parametrization (no detector dependent at first order)
        std::cout<<"Loading the VUV time parametrization"<<std::endl;
        fDistances_all     = p.get<std::vector<double> >("Distances_landau");
        fNorm_over_entries = p.get<std::vector<double> >("Norm_over_entries");
        fMpv = p.get<std::vector<double> >("Mpv");
        fWidth = p.get<std::vector<double> >("Width");
        fDistances = p.get<std::vector<double> >("Distances_exp");
        fSlope = p.get<std::vector<double> >("Slope");
        fExpo_over_Landau_norm[0] = p.get<std::vector<double> >("Expo_over_Landau_norm_0");
        fExpo_over_Landau_norm[1] = p.get<std::vector<double> >("Expo_over_Landau_norm_30");
        fExpo_over_Landau_norm[2] = p.get<std::vector<double> >("Expo_over_Landau_norm_60");
        fstep_size = p.get<double>("step_size");
        fmax_d= p.get<double>("max_d");
        fvuv_vgroup_mean= p.get<double>("vuv_vgroup_mean");
        fvuv_vgroup_max= p.get<double>("vuv_vgroup_max");
        finflexion_point_distance= p.get<double>("inflexion_point_distance");

        if (fStoreReflected)
        {

          // load VIS arrival time distribution paramterisation
          std::cout << "Loading the VIS time paramterisation" << std::endl;
          fDistances_refl = p.get<std::vector<double>> ("Distances_refl");
          fCut_off = p.get<std::vector<std::vector<double>>> ("Cut_off");
          fTau = p.get<std::vector<std::vector<double>>> ("Tau");
          fvis_vmean = p.get<double> ("vis_vmean");
          fn_LAr_VUV = p.get<double> ("n_LAr_VUV");
          fn_LAr_vis = p.get<double> ("n_LAr_vis");

        }
      }

    if(fUseNhitsModel) {
        std::cout << "Loading semi-analytic mode models" << std::endl;
        // VUV
        fGH_PARS = p.get<std::vector<std::vector<double> > >("GH_PARS");
	std::vector<double> v0(2,0.0);
	fBORDER_correction = p.get<std::vector<double>>("BORDER_correction", v0);
        if (fStoreReflected)
        {
        // VIS
        fVIS_PARS = p.get<std::vector<std::vector<double>>>("VIS_PARS");
	// VIS border correction
	if (fApplyVISBorderCorrection)
	{
	std::vector<double> vx(19,0.0);
	std::vector<double> vr(5,0.0);
	std::vector<std::vector<double>> vxr(5,vx);
	std::vector<std::vector<std::vector<double>>> vc(9,vxr);
	fVIS_BORDER_distances_x = p.get< std::vector<double> > ("VIS_BORDER_distances_x", vx);
        fVIS_BORDER_distances_r = p.get< std::vector<double> > ("VIS_BORDER_distances_r", vr);
	fVIS_BORDER_correction  = p.get< std::vector<std::vector<std::vector<double>>> > ("VIS_BORDER_correction", vc);
        }
        }
	// optical detector information
	fPMT_radius = p.get<double>("PMT_radius", 10.16);
    }

    return;

  }


  //------------------------------------------------------
  void PhotonVisibilityService::PhotonLibraryOnlyParametersCheck() const {
    
    std::optional<cet::exception> e;
    auto errMsg =
      [&e](){ if (!e) e.emplace("PhotonVisibilityService"); return e.value(); };
    if (!fSaveAsBinaryFile.empty()) {
      errMsg() << "Option 'SaveAsBinaryFile' (set to '" << fSaveAsBinaryFile
        << "' is valid only when *reading* a *standard* photon library"
        << " (e.g. not an hybrid one).";
    }
    if (!fLoadFromBinaryFile.empty()) {
      errMsg() << "Option 'LoadFromBinaryFile' (set to '" << fLoadFromBinaryFile
        << "' is valid only when *reading* a *standard* photon library"
        << " (e.g. not an hybrid one).";
    }
    if (e) throw std::move(e.value());
  } // PhotonVisibilityService::PhotonLibraryOnlyParametersCheck()


  //------------------------------------------------------

  // Eventually we will calculate the light quenching factor here
  double PhotonVisibilityService::GetQuenchingFactor(double /* dQdx */) const
  {
    // for now, no quenching
    return 1.0;

  }


  //------------------------------------------------------

  // Get a vector of the relative visibilities of each OpDet
  //  in the event to a point p

  auto PhotonVisibilityService::doGetAllVisibilities(geo::Point_t const& p, bool wantReflected) const -> MappedCounts_t
  {
    phot::IPhotonLibrary::Counts_t data{};

    // first we fill a container of visibilities in the library index space
    // (it is directly the values of the library unless interpolation is
    //  requested)
    if(fInterpolate){
      // this is a punch into multithreading face:
      static std::vector<float> ret;
      ret.resize(fMapping->libraryMappingSize(p));
      for(std::size_t libIndex = 0; libIndex < ret.size(); ++libIndex) {
        ret[libIndex]
          = doGetVisibilityOfOpLib(p, LibraryIndex_t(libIndex), wantReflected);
      }
      data = &ret.front();
    }
    else{
      auto const VoxID = VoxelAt(p);
      data = GetLibraryEntries(VoxID, wantReflected);
    }
    return fMapping->applyOpDetMapping(p, std::move(data));
  }


  //------------------------------------------------------

  // Get distance to optical detector OpDet
  double PhotonVisibilityService::DistanceToOpDetImpl( geo::Point_t const& p, unsigned int OpDet )
  {
    art::ServiceHandle<geo::Geometry const> geom;
    return geom->OpDetGeoFromOpDet(OpDet).DistanceToPoint(p);
  }


  //------------------------------------------------------


  // Get the solid angle reduction factor for planar optical detector OpDet
  double PhotonVisibilityService::SolidAngleFactorImpl( geo::Point_t const& p, unsigned int OpDet )
  {
    art::ServiceHandle<geo::Geometry const> geom;
    return geom->OpDetGeoFromOpDet(OpDet).CosThetaFromNormal(p);
  }

  //------------------------------------------------------

  float PhotonVisibilityService::doGetVisibilityOfOpLib
    (geo::Point_t const& p, LibraryIndex_t libIndex, bool wantReflected /* = false */) const
  {
    if(!fInterpolate) {
      return GetLibraryEntry(VoxelAt(p), libIndex, wantReflected);
    }

    // In case we're outside the bounding box we'll get a empty optional list.
    auto const neis = fVoxelDef.GetNeighboringVoxelIDs(LibLocation(p));
    if (!neis) return 0.0;

    // Sum up all the weighted neighbours to get interpolation behaviour
    float vis = 0.0;
    for(const sim::PhotonVoxelDef::NeiInfo& n: neis.value()) {
      if (n.id < 0) continue;
      vis += n.weight * GetLibraryEntry(n.id, libIndex, wantReflected);
    }
    return vis;
  }


  //------------------------------------------------------

  bool PhotonVisibilityService::doHasVisibility
    (geo::Point_t const& p, bool wantReflected /* = false */) const
    { return HasLibraryEntries(VoxelAt(p), wantReflected); }

  //------------------------------------------------------

  float PhotonVisibilityService::doGetVisibility(geo::Point_t const& p, unsigned int OpChannel, bool wantReflected) const
  {
    // here we quietly confuse op. det. channel (interface) and op. det. (library)
    LibraryIndex_t const libIndex = fMapping->opDetToLibraryIndex(p, OpChannel);
    return doGetVisibilityOfOpLib(p, libIndex, wantReflected);
  }


  //------------------------------------------------------

  void PhotonVisibilityService::StoreLightProd(int VoxID, double N)
  {
    fCurrentVoxel = VoxID;
    fCurrentValue = N;
    mf::LogInfo("PhotonVisibilityService") << " PVS notes production of " << N << " photons at Vox " << VoxID<<std::endl;
  }


  //------------------------------------------------------


  void PhotonVisibilityService::RetrieveLightProd(int& VoxID, double& N) const
  {
    N     = fCurrentValue;
    VoxID = fCurrentVoxel;
  }

  //------------------------------------------------------

  void PhotonVisibilityService::SetLibraryEntry(int VoxID, int OpChannel, float N, bool wantReflected)
  {
    if(fTheLibrary == 0)
      LoadLibrary();

    PhotonLibrary* lib = dynamic_cast<PhotonLibrary*>(fTheLibrary);

    if(!wantReflected)
    lib->SetCount(VoxID,OpChannel, N);

    else
    lib->SetReflCount(VoxID,OpChannel, N);

    //std::cout<< " PVS logging " << VoxID << " " << OpChannel<<std::endl;
    MF_LOG_DEBUG("PhotonVisibilityService") << " PVS logging " << VoxID << " " << OpChannel<<std::endl;
  }

  //------------------------------------------------------

  phot::IPhotonLibrary::Counts_t PhotonVisibilityService::GetLibraryEntries(int VoxID, bool wantReflected) const
  {
    if(fTheLibrary == 0)
      LoadLibrary();

    if(!wantReflected)
      return fTheLibrary->GetCounts(VoxID);
    else
      return fTheLibrary->GetReflCounts(VoxID);
  }

  //------------------------------------------------------

  bool PhotonVisibilityService::HasLibraryEntries(int VoxID, bool /* wantReflected */ /* = false */) const
  {
    if (!fTheLibrary) LoadLibrary();
    return fTheLibrary->isVoxelValid(VoxID);
  }

  //------------------------------------------------------

  float PhotonVisibilityService::GetLibraryEntry(int VoxID, OpDetID_t libOpChannel, bool wantReflected) const
  {
    if(fTheLibrary == 0)
      LoadLibrary();

    if(!wantReflected)
      return fTheLibrary->GetCount(VoxID, libOpChannel);
    else
      return fTheLibrary->GetReflCount(VoxID, libOpChannel);
  }
  // Methodes to handle the extra library parameter refl T0
  //------------------------------------------------------

  // Get a vector of the refl <tfirst> of each OpDet
  //  in the event to a point p

  auto PhotonVisibilityService::doGetReflT0s(geo::Point_t const& p) const -> MappedT0s_t
  {
    // both the input and the output go through mapping to apply needed symmetries.
    int const VoxID = VoxelAt(p);
    return fMapping->applyOpDetMapping(p, GetLibraryReflT0Entries(VoxID));
  }

  //------------------------------------------------------

  phot::IPhotonLibrary::Counts_t PhotonVisibilityService::GetLibraryReflT0Entries(int VoxID) const
  {
    if(fTheLibrary == 0)
      LoadLibrary();

    return fTheLibrary->GetReflT0s(VoxID);
  }

  //------------------------------------------------------

  void PhotonVisibilityService::SetLibraryReflT0Entry(int VoxID, int OpChannel, float T0)
  {
    PhotonLibrary* lib = dynamic_cast<PhotonLibrary*>(fTheLibrary);
    if(fTheLibrary == 0)
      LoadLibrary();

    lib->SetReflT0(VoxID,OpChannel,T0);

    MF_LOG_DEBUG("PhotonVisibilityService") << " PVS logging " << VoxID << " " << OpChannel<<std::endl;
  }

  //------------------------------------------------------

  float PhotonVisibilityService::GetLibraryReflT0Entry(int VoxID, OpDetID_t libOpChannel) const
  {
    if(fTheLibrary == 0)
      LoadLibrary();

    return fTheLibrary->GetReflT0(VoxID, libOpChannel);
  }

  //------------------------------------------------------


/////////////****////////////

  auto PhotonVisibilityService::doGetTimingPar(geo::Point_t const& p) const -> MappedParams_t
  {
    int const VoxID = VoxelAt(p);
    return fMapping->applyOpDetMapping(p, GetLibraryTimingParEntries(VoxID));
  }

  auto PhotonVisibilityService::doGetTimingTF1(geo::Point_t const& p) const -> MappedFunctions_t
  {
    int const VoxID = VoxelAt(p);
    return fMapping->applyOpDetMapping(p, GetLibraryTimingTF1Entries(VoxID));
  }


  //------------------------------------------------------

  phot::IPhotonLibrary::Params_t PhotonVisibilityService::GetLibraryTimingParEntries(int VoxID) const
  {
    PhotonLibrary* lib = dynamic_cast<PhotonLibrary*>(fTheLibrary);
    if(fTheLibrary == 0)
      LoadLibrary();

    return lib->GetTimingPars(VoxID);
  }

  //------------------------------------------------------

  phot::IPhotonLibrary::Functions_t PhotonVisibilityService::GetLibraryTimingTF1Entries(int VoxID) const
  {
    PhotonLibrary* lib = dynamic_cast<PhotonLibrary*>(fTheLibrary);
    if(fTheLibrary == 0)
      LoadLibrary();

    return lib->GetTimingTF1s(VoxID);
  }

  //------------------------------------------------------

  void PhotonVisibilityService::SetLibraryTimingParEntry(int VoxID, int OpChannel, float par, size_t parnum)
  {
    PhotonLibrary* lib = dynamic_cast<PhotonLibrary*>(fTheLibrary);
    if(fTheLibrary == 0)
      LoadLibrary();

    lib->SetTimingPar(VoxID,OpChannel,par, parnum);

    MF_LOG_DEBUG("PhotonVisibilityService") << " PVS logging " << VoxID << " " << OpChannel<<std::endl;
  }

  //------------------------------------------------------

  void PhotonVisibilityService::SetLibraryTimingTF1Entry(int VoxID, int OpChannel, TF1 const& func)
  {
    PhotonLibrary* lib = dynamic_cast<PhotonLibrary*>(fTheLibrary);
    if(fTheLibrary == 0)
      LoadLibrary();

    lib->SetTimingTF1(VoxID,OpChannel,func);

    MF_LOG_DEBUG("PhotonVisibilityService") << " PVS logging " << VoxID << " " << OpChannel<<std::endl;
  }


  //------------------------------------------------------

  float PhotonVisibilityService::GetLibraryTimingParEntry(int VoxID, OpDetID_t libOpChannel, size_t npar) const
  {
    PhotonLibrary* lib = dynamic_cast<PhotonLibrary*>(fTheLibrary);
    if(fTheLibrary == 0)
      LoadLibrary();

    return lib->GetTimingPar(VoxID, libOpChannel, npar);
  }

  //------------------------------------------------------

  size_t PhotonVisibilityService::NOpChannels() const
  {
    // the last word about the number of channels belongs to the mapping;
    // this should be also the same answer as `geo::GeometryCore::NOpDets()`.
    return fMapping->opDetMappingSize();
  }

  //------------------------------------------------------
  void PhotonVisibilityService::SetDirectLightPropFunctions(TF1 const* functions[8], double& d_break, double& d_max, double& tf1_sampling_factor) const
  {
    functions[0] = fparslogNorm;
    functions[1] = fparsMPV;
    functions[2] = fparsWidth;
    functions[3] = fparsCte;
    functions[4] = fparsSlope;
    functions[5] = fparslogNorm_far;
    functions[6] = fparsMPV_far;
    functions[7] = fparsCte_far;

    d_break = fD_break;
    d_max = fD_max;
    tf1_sampling_factor = fTF1_sampling_factor;
  }

  //------------------------------------------------------
  void PhotonVisibilityService::SetReflectedCOLightPropFunctions(TF1 const* functions[5], double& t0_max, double& t0_break_point) const
  {
    functions[0] = fparslogNorm_refl;
    functions[1] = fparsMPV_refl;
    functions[2] = fparsWidth_refl;
    functions[3] = fparsCte_refl;
    functions[4] = fparsSlope_refl;

    t0_max = fT0_max;
    t0_break_point = fT0_break_point;
  }



  //------------------------------------------------------
  void PhotonVisibilityService::LoadTimingsForVUVPar(std::vector<double> v[9], double& step_size, double& max_d,
                                                     double& vuv_vgroup_mean, double& vuv_vgroup_max, double& inflexion_point_distance) const
  {
    v[0] = fDistances_all;
    v[1] = fNorm_over_entries;
    v[2] = fMpv;
    v[3] = fWidth;
    v[4] = fDistances;
    v[5] = fSlope;
    v[6] = fExpo_over_Landau_norm[0];
    v[7] = fExpo_over_Landau_norm[1];
    v[8] = fExpo_over_Landau_norm[2];

    step_size = fstep_size;
    max_d = fmax_d;
    vuv_vgroup_mean = fvuv_vgroup_mean;
    vuv_vgroup_max = fvuv_vgroup_max;
    inflexion_point_distance = finflexion_point_distance;

  }

  void PhotonVisibilityService::LoadTimingsForVISPar(std::vector<double>& distances, std::vector<std::vector<double>>& cut_off, std::vector<std::vector<double>>& tau,
                                                                                 double& vis_vmean, double& n_vis, double& n_vuv) const
  {
  distances = fDistances_refl;
  cut_off = fCut_off;
  tau = fTau;

  vis_vmean = fvis_vmean;
  n_vis = fn_LAr_vis;
  n_vuv = fn_LAr_VUV;

  }


  void PhotonVisibilityService::LoadGHForVUVCorrection(std::vector<std::vector<double>>& v, std::vector<double>& border, double& r) const
  {

    v = fGH_PARS;
    border = fBORDER_correction;
    r = fPMT_radius;

  }

  void PhotonVisibilityService::LoadParsForVISCorrection(std::vector<std::vector<double>>& v, double& r) const
  {
    v = fVIS_PARS;
    r = fPMT_radius;
  }
  
  void PhotonVisibilityService::LoadParsForVISBorderCorrection(std::vector<double>& border_distances_x, std::vector<double>& border_distances_r, std::vector<std::vector<std::vector<double>>>& border_correction) const
 {
    border_distances_x = fVIS_BORDER_distances_x;
    border_distances_r = fVIS_BORDER_distances_r;
    border_correction = fVIS_BORDER_correction;
 }


  //------------------------------------------------------
  /***
   * Preform any necessary transformations on the coordinates before trying to access
   * a voxel ID.
   **/
  geo::Point_t PhotonVisibilityService::LibLocation(geo::Point_t const& p) const
  {
    return fMapping->detectorToLibrary(p);
  }

} // namespace

namespace phot{

  DEFINE_ART_SERVICE(PhotonVisibilityService)

} // namespace phot
