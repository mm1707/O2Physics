// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file   flowGfwOmegaXi.cxx
/// \author Fuchun Cui(fcui@cern.ch)
/// \since  Sep/13/2024
/// \brief  This task is to caculate V0s and cascades flow by GenericFramework

#include <CCDB/BasicCCDBManager.h>
#include <vector>
#include <string>
#include <cmath>
#include <memory>
#include "Framework/runDataProcessing.h"
#include "Framework/AnalysisTask.h"
#include "Framework/ASoAHelpers.h"
#include "Framework/RunningWorkflowInfo.h"
#include "Framework/HistogramRegistry.h"
#include "Common/DataModel/EventSelection.h"
#include "Common/Core/TrackSelection.h"
#include "Common/DataModel/TrackSelectionTables.h"
#include "Common/DataModel/Centrality.h"
#include "Common/DataModel/PIDResponse.h"
#include "Common/DataModel/Multiplicity.h"
#include "GFWPowerArray.h"
#include "GFW.h"
#include "GFWCumulant.h"
#include "GFWWeights.h"
#include "Common/DataModel/Qvectors.h"
#include "Common/Core/EventPlaneHelper.h"
#include "ReconstructionDataFormats/Track.h"
#include "CommonConstants/PhysicsConstants.h"
#include "Common/Core/trackUtilities.h"
#include "PWGLF/DataModel/LFStrangenessTables.h"
#include "PWGMM/Mult/DataModel/Index.h"
#include "TList.h"
#include <TProfile.h>
#include <TRandom3.h>
#include <TF1.h>
#include <TF2.h>
#include <TPDGCode.h>

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;
namespace
{
std::shared_ptr<TProfile> refc22[20];
std::shared_ptr<TProfile> refc24[20];
std::shared_ptr<TProfile3D> k0sc22[20];
std::shared_ptr<TProfile3D> k0sc24[20];
std::shared_ptr<TProfile3D> lambdac22[20];
std::shared_ptr<TProfile3D> lambdac24[20];
std::shared_ptr<TProfile3D> xic22[20];
std::shared_ptr<TProfile3D> xic24[20];
std::shared_ptr<TProfile3D> omegac22[20];
std::shared_ptr<TProfile3D> omegac24[20];
} // namespace

#define O2_DEFINE_CONFIGURABLE(NAME, TYPE, DEFAULT, HELP) Configurable<TYPE> NAME{#NAME, DEFAULT, HELP};

struct FlowGfwOmegaXi {

  O2_DEFINE_CONFIGURABLE(cfgCutVertex, float, 10.0f, "Accepted z-vertex range")
  O2_DEFINE_CONFIGURABLE(cfgCutPtPOIMin, float, 0.2f, "Minimal pT for poi tracks")
  O2_DEFINE_CONFIGURABLE(cfgCutPtPOIMax, float, 10.0f, "Maximal pT for poi tracks")
  O2_DEFINE_CONFIGURABLE(cfgCutPtMin, float, 0.2f, "Minimal pT for ref tracks")
  O2_DEFINE_CONFIGURABLE(cfgCutPtMax, float, 10.0f, "Maximal pT for ref tracks")
  O2_DEFINE_CONFIGURABLE(cfgCutEta, float, 0.8f, "Eta range for tracks")
  O2_DEFINE_CONFIGURABLE(cfgCutChi2prTPCcls, float, 2.5, "Chi2 per TPC clusters")
  O2_DEFINE_CONFIGURABLE(cfgCutOccupancyHigh, int, 500, "High cut on TPC occupancy")
  O2_DEFINE_CONFIGURABLE(cfgMassBins, std::vector<int>, (std::vector<int>{80, 32, 14, 16}), "Number of K0s, Lambda, Xi, Omega mass axis bins for c22")
  O2_DEFINE_CONFIGURABLE(cfgDeltaPhiLocDen, int, 3, "Number of delta phi for local density, 200 bins in 2 pi")
  // topological cut for V0
  O2_DEFINE_CONFIGURABLE(cfgv0_radius, float, 5.0f, "minimum decay radius")
  O2_DEFINE_CONFIGURABLE(cfgv0_v0cospa, float, 0.995f, "minimum cosine of pointing angle")
  O2_DEFINE_CONFIGURABLE(cfgv0_dcadautopv, float, 0.1f, "minimum daughter DCA to PV")
  O2_DEFINE_CONFIGURABLE(cfgv0_dcav0dau, float, 0.5f, "maximum DCA among V0 daughters")
  O2_DEFINE_CONFIGURABLE(cfgv0_mk0swindow, float, 0.1f, "Invariant mass window of K0s")
  O2_DEFINE_CONFIGURABLE(cfgv0_mlambdawindow, float, 0.04f, "Invariant mass window of lambda")
  O2_DEFINE_CONFIGURABLE(cfgv0_ArmPodocut, float, 0.2f, "Armenteros Podolski cut for K0")
  // topological cut for cascade
  O2_DEFINE_CONFIGURABLE(cfgcasc_radius, float, 0.5f, "minimum decay radius")
  O2_DEFINE_CONFIGURABLE(cfgcasc_casccospa, float, 0.999f, "minimum cosine of pointing angle")
  O2_DEFINE_CONFIGURABLE(cfgcasc_v0cospa, float, 0.998f, "minimum cosine of pointing angle")
  O2_DEFINE_CONFIGURABLE(cfgcasc_dcav0topv, float, 0.01f, "minimum daughter DCA to PV")
  O2_DEFINE_CONFIGURABLE(cfgcasc_dcabachtopv, float, 0.01f, "minimum bachelor DCA to PV")
  O2_DEFINE_CONFIGURABLE(cfgcasc_dcacascdau, float, 0.3f, "maximum DCA among cascade daughters")
  O2_DEFINE_CONFIGURABLE(cfgcasc_dcav0dau, float, 1.0f, "maximum DCA among V0 daughters")
  O2_DEFINE_CONFIGURABLE(cfgcasc_mlambdawindow, float, 0.04f, "Invariant mass window of lambda")
  // track quality and type selections
  O2_DEFINE_CONFIGURABLE(cfgtpcclusters, int, 70, "minimum number of TPC clusters requirement")
  O2_DEFINE_CONFIGURABLE(cfgitsclusters, int, 1, "minimum number of ITS clusters requirement")
  O2_DEFINE_CONFIGURABLE(cfgtpcclufindable, int, 1, "minimum number of findable TPC clusters")
  O2_DEFINE_CONFIGURABLE(cfgtpccrossoverfindable, int, 1, "minimum number of Ratio crossed rows over findable clusters")
  O2_DEFINE_CONFIGURABLE(cfgCasc_rapidity, float, 0.5, "rapidity")
  O2_DEFINE_CONFIGURABLE(cfgNSigmatpctof, std::vector<float>, (std::vector<float>{3, 3, 3, 3, 3, 3}), "tpc and tof NSigma for Pion Proton Kaon")
  O2_DEFINE_CONFIGURABLE(cfgAcceptancePath, std::vector<std::string>, (std::vector<std::string>{"Users/f/fcui/NUA/NUAREFPartical", "Users/f/fcui/NUA/NUAK0s", "Users/f/fcui/NUA/NUALambda", "Users/f/fcui/NUA/NUAXi", "Users/f/fcui/NUA/NUAOmega"}), "CCDB path to acceptance object")
  O2_DEFINE_CONFIGURABLE(cfgEfficiencyPath, std::vector<std::string>, (std::vector<std::string>{"PathtoRef"}), "CCDB path to efficiency object")
  O2_DEFINE_CONFIGURABLE(cfgLocDenParaXi, std::vector<double>, (std::vector<double>{-0.000986187, -3.86861, -0.000912481, -3.29206, -0.000859271, -2.89389, -0.000817039, -2.61201, -0.000788792, -2.39079, -0.000780182, -2.19276, -0.000750457, -2.07205, -0.000720279, -1.96865, -0.00073247, -1.85642, -0.000695091, -1.82625, -0.000693332, -1.72679, -0.000681225, -1.74305, -0.000652818, -1.92608, -0.000618892, -2.31985}), "Local density efficiency function parameter for Xi, exp(Ax + B)")
  O2_DEFINE_CONFIGURABLE(cfgLocDenParaOmega, std::vector<double>, (std::vector<double>{-0.000444324, -6.0424, -0.000566208, -5.42168, -0.000580338, -4.96967, -0.000721054, -4.41994, -0.000626394, -4.27934, -0.000652167, -3.9543, -0.000592327, -3.79053, -0.000544721, -3.73292, -0.000613419, -3.43849, -0.000402506, -3.47687, -0.000602687, -3.24491, -0.000460848, -3.056, -0.00039428, -2.35188, -0.00041908, -2.03642}), "Local density efficiency function parameter for Omega, exp(Ax + B)")
  O2_DEFINE_CONFIGURABLE(cfgLocDenParaK0s, std::vector<double>, (std::vector<double>{-0.00043057, -3.2435, -0.000385085, -2.97687, -0.000350298, -2.81502, -0.000326159, -2.71091, -0.000299563, -2.65448, -0.000294284, -2.60865, -0.000277938, -2.589, -0.000277091, -2.56983, -0.000272783, -2.56825, -0.000252706, -2.58996, -0.000247834, -2.63158, -0.00024379, -2.76976, -0.000286468, -2.92484, -0.000310149, -3.27746}), "Local density efficiency function parameter for K0s, exp(Ax + B)")
  O2_DEFINE_CONFIGURABLE(cfgLocDenParaLambda, std::vector<double>, (std::vector<double>{-0.000510948, -4.4846, -0.000460629, -4.14465, -0.000433729, -3.94173, -0.000412751, -3.81839, -0.000411211, -3.72502, -0.000401511, -3.68426, -0.000407461, -3.67005, -0.000379371, -3.71153, -0.000392828, -3.73214, -0.000403996, -3.80717, -0.000403376, -3.90917, -0.000354624, -4.34629, -0.000477606, -4.66307, -0.000541139, -4.61364}), "Local density efficiency function parameter for Lambda, exp(Ax + B)")
  // switch
  O2_DEFINE_CONFIGURABLE(cfgcheckDauTPC, bool, true, "check daughter tracks TPC or not")
  O2_DEFINE_CONFIGURABLE(cfgcheckDauTOF, bool, false, "check daughter tracks TOF or not")
  O2_DEFINE_CONFIGURABLE(cfgDoAccEffCorr, bool, false, "do acc and eff corr")
  O2_DEFINE_CONFIGURABLE(cfgDoLocDenCorr, bool, false, "do local density corr")
  O2_DEFINE_CONFIGURABLE(cfgDoJackknife, bool, false, "do jackknife")
  O2_DEFINE_CONFIGURABLE(cfgOutputNUAWeights, bool, false, "Fill and output NUA weights")
  O2_DEFINE_CONFIGURABLE(cfgOutputLocDenWeights, bool, false, "Fill and output local density weights")

  ConfigurableAxis cfgaxisVertex{"cfgaxisVertex", {20, -10, 10}, "vertex axis for histograms"};
  ConfigurableAxis cfgaxisPhi{"cfgaxisPhi", {60, 0.0, constants::math::TwoPI}, "phi axis for histograms"};
  ConfigurableAxis cfgaxisEta{"cfgaxisEta", {40, -1., 1.}, "eta axis for histograms"};
  ConfigurableAxis cfgaxisPt{"cfgaxisPt", {VARIABLE_WIDTH, 0.20, 0.25, 0.30, 0.35, 0.40, 0.45, 0.50, 0.55, 0.60, 0.65, 0.70, 0.75, 0.80, 0.85, 0.90, 0.95, 1.00, 1.10, 1.20, 1.30, 1.40, 1.50, 1.60, 1.70, 1.80, 1.90, 2.00, 2.20, 2.40, 2.60, 2.80, 3.00, 3.50, 4.00, 4.50, 5.00, 5.50, 6.00, 10.0}, "pt (GeV)"};
  ConfigurableAxis cfgaxisPtXi{"cfgaxisPtXi", {VARIABLE_WIDTH, 0.9, 1.1, 1.3, 1.5, 1.7, 1.9, 2.1, 2.3, 2.5, 2.7, 2.9, 3.9, 4.9, 5.9, 9.9}, "pt (GeV)"};
  ConfigurableAxis cfgaxisPtOmega{"cfgaxisPtOmega", {VARIABLE_WIDTH, 0.9, 1.1, 1.3, 1.5, 1.7, 1.9, 2.1, 2.3, 2.5, 2.7, 2.9, 3.9, 4.9, 5.9, 9.9}, "pt (GeV)"};
  ConfigurableAxis cfgaxisPtV0{"cfgaxisPtV0", {VARIABLE_WIDTH, 0.9, 1.1, 1.3, 1.5, 1.7, 1.9, 2.1, 2.3, 2.5, 2.7, 2.9, 3.9, 4.9, 5.9, 9.9}, "pt (GeV)"};
  ConfigurableAxis cfgaxisOmegaMassforflow{"cfgaxisOmegaMassforflow", {16, 1.63f, 1.71f}, "Inv. Mass (GeV)"};
  ConfigurableAxis cfgaxisXiMassforflow{"cfgaxisXiMassforflow", {14, 1.3f, 1.37f}, "Inv. Mass (GeV)"};
  ConfigurableAxis cfgaxisK0sMassforflow{"cfgaxisK0sMassforflow", {40, 0.4f, 0.6f}, "Inv. Mass (GeV)"};
  ConfigurableAxis cfgaxisLambdaMassforflow{"cfgaxisLambdaMassforflow", {32, 1.08f, 1.16f}, "Inv. Mass (GeV)"};
  ConfigurableAxis cfgaxisNch{"cfgaxisNch", {3000, 0.5, 3000.5}, "Nch"};
  ConfigurableAxis cfgaxisLocalDensity{"cfgaxisLocalDensity", {200, 0, 600}, "local density"};

  AxisSpec axisMultiplicity{{0, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90}, "Centrality (%)"};
  AxisSpec axisOmegaMass = {80, 1.63f, 1.71f, "Inv. Mass (GeV)"};
  AxisSpec axisXiMass = {70, 1.3f, 1.37f, "Inv. Mass (GeV)"};
  AxisSpec axisK0sMass = {400, 0.4f, 0.6f, "Inv. Mass (GeV)"};
  AxisSpec axisLambdaMass = {160, 1.08f, 1.16f, "Inv. Mass (GeV)"};

  Filter collisionFilter = nabs(aod::collision::posZ) < cfgCutVertex;
  Filter trackFilter = (nabs(aod::track::eta) < cfgCutEta) && (aod::track::pt > cfgCutPtPOIMin) && (aod::track::pt < cfgCutPtPOIMax) && ((requireGlobalTrackInFilter()) || (aod::track::isGlobalTrackSDD == (uint8_t) true)) && (aod::track::tpcChi2NCl < cfgCutChi2prTPCcls);

  using TracksPID = soa::Join<aod::pidTPCPi, aod::pidTPCKa, aod::pidTPCPr, aod::pidTOFPi, aod::pidTOFKa, aod::pidTOFPr>;
  using AodTracks = soa::Filtered<soa::Join<aod::Tracks, aod::TrackSelection, aod::TracksExtra, TracksPID>>; // tracks filter
  using AodCollisions = soa::Filtered<soa::Join<aod::Collisions, aod::EvSels, aod::CentFT0Cs, aod::Mults>>;  // collisions filter
  using DaughterTracks = soa::Join<aod::Tracks, aod::TracksExtra, aod::TracksDCA, TracksPID>;

  // Connect to ccdb
  Service<ccdb::BasicCCDBManager> ccdb;
  O2_DEFINE_CONFIGURABLE(cfgnolaterthan, int64_t, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(), "latest acceptable timestamp of creation for the object")
  O2_DEFINE_CONFIGURABLE(cfgurl, std::string, "http://alice-ccdb.cern.ch", "url of the ccdb repository")

  // Define output
  HistogramRegistry registry{"registry"};
  OutputObj<GFWWeights> fWeightsREF{GFWWeights("weightsREF")};
  OutputObj<GFWWeights> fWeightsK0s{GFWWeights("weightsK0s")};
  OutputObj<GFWWeights> fWeightsLambda{GFWWeights("weightsLambda")};
  OutputObj<GFWWeights> fWeightsXi{GFWWeights("weightsXi")};
  OutputObj<GFWWeights> fWeightsOmega{GFWWeights("weightsOmega")};

  // define global variables
  GFW* fGFW = new GFW(); // GFW class used from main src
  std::vector<GFW::CorrConfig> corrconfigs;
  std::vector<std::string> cfgAcceptance = cfgAcceptancePath;
  std::vector<std::string> cfgEfficiency = cfgEfficiencyPath;
  std::vector<float> cfgNSigma = cfgNSigmatpctof;
  std::vector<int> cfgmassbins = cfgMassBins;

  std::vector<GFWWeights*> mAcceptance;
  std::vector<TH1D*> mEfficiency;
  bool correctionsLoaded = false;

  TF1* fMultPVCutLow = nullptr;
  TF1* fMultPVCutHigh = nullptr;
  TF1* fT0AV0AMean = nullptr;
  TF1* fT0AV0ASigma = nullptr;

  // Declare the pt, mult and phi Axis;
  int nPtBins = 0;
  TAxis* fPtAxis = nullptr;

  int nXiPtBins = 0;
  TAxis* fXiPtAxis = nullptr;

  int nV0PtBins = 0;
  TAxis* fV0PtAxis = nullptr;

  TAxis* fMultAxis = nullptr;

  TAxis* fOmegaMass = nullptr;

  TAxis* fXiMass = nullptr;

  TAxis* fK0sMass = nullptr;

  TAxis* fLambdaMass = nullptr;

  void init(InitContext const&) // Initialization
  {
    ccdb->setURL(cfgurl.value);
    ccdb->setCaching(true);
    ccdb->setCreatedNotAfter(cfgnolaterthan.value);

    // Set the pt, mult and phi Axis;
    o2::framework::AxisSpec axisPt = cfgaxisPt;
    nPtBins = axisPt.binEdges.size() - 1;
    fPtAxis = new TAxis(nPtBins, &(axisPt.binEdges)[0]);

    o2::framework::AxisSpec axisXiPt = cfgaxisPtXi;
    nXiPtBins = axisXiPt.binEdges.size() - 1;
    fXiPtAxis = new TAxis(nXiPtBins, &(axisXiPt.binEdges)[0]);

    o2::framework::AxisSpec axisV0Pt = cfgaxisPtV0;
    nV0PtBins = axisV0Pt.binEdges.size() - 1;
    fV0PtAxis = new TAxis(nV0PtBins, &(axisV0Pt.binEdges)[0]);

    o2::framework::AxisSpec axisMult = axisMultiplicity;
    int nMultBins = axisMult.binEdges.size() - 1;
    fMultAxis = new TAxis(nMultBins, &(axisMult.binEdges)[0]);

    fOmegaMass = new TAxis(cfgmassbins[3], 1.63, 1.71);

    fXiMass = new TAxis(cfgmassbins[2], 1.3, 1.37);

    fK0sMass = new TAxis(cfgmassbins[0], 0.4, 0.6);

    fLambdaMass = new TAxis(cfgmassbins[1], 1.08, 1.16);

    // Add some output objects to the histogram registry
    registry.add("hPhi", "", {HistType::kTH1D, {cfgaxisPhi}});
    registry.add("hPhicorr", "", {HistType::kTH1D, {cfgaxisPhi}});
    registry.add("hEta", "", {HistType::kTH1D, {cfgaxisEta}});
    registry.add("hVtxZ", "", {HistType::kTH1D, {cfgaxisVertex}});
    registry.add("hMult", "", {HistType::kTH1D, {cfgaxisNch}});
    registry.add("hCent", "", {HistType::kTH1D, {{90, 0, 90}}});
    registry.add("hCentvsNch", "", {HistType::kTH2D, {{18, 0, 90}, cfgaxisNch}});
    registry.add("MC/hCentvsNchMC", "", {HistType::kTH2D, {{18, 0, 90}, cfgaxisNch}});
    registry.add("hPt", "", {HistType::kTH1D, {cfgaxisPt}});
    registry.add("hEtaPhiVtxzREF", "", {HistType::kTH3D, {cfgaxisPhi, cfgaxisEta, {20, -10, 10}}});
    registry.add("hEtaPhiVtxzPOIXi", "", {HistType::kTH3D, {cfgaxisPhi, cfgaxisEta, {20, -10, 10}}});
    registry.add("hEtaPhiVtxzPOIOmega", "", {HistType::kTH3D, {cfgaxisPhi, cfgaxisEta, {20, -10, 10}}});
    registry.add("hEtaPhiVtxzPOIK0s", "", {HistType::kTH3D, {cfgaxisPhi, cfgaxisEta, {20, -10, 10}}});
    registry.add("hEtaPhiVtxzPOILambda", "", {HistType::kTH3D, {cfgaxisPhi, cfgaxisEta, {20, -10, 10}}});
    registry.add("hEventCount", "", {HistType::kTH2D, {{4, 0, 4}, {4, 0, 4}}});
    registry.get<TH2>(HIST("hEventCount"))->GetXaxis()->SetBinLabel(1, "Filtered event");
    registry.get<TH2>(HIST("hEventCount"))->GetXaxis()->SetBinLabel(2, "after sel8");
    registry.get<TH2>(HIST("hEventCount"))->GetXaxis()->SetBinLabel(3, "before topological cut");
    registry.get<TH2>(HIST("hEventCount"))->GetXaxis()->SetBinLabel(4, "after topological cut");
    registry.get<TH2>(HIST("hEventCount"))->GetYaxis()->SetBinLabel(1, "K0s");
    registry.get<TH2>(HIST("hEventCount"))->GetYaxis()->SetBinLabel(2, "Lambda");
    registry.get<TH2>(HIST("hEventCount"))->GetYaxis()->SetBinLabel(3, "XiMinus");
    registry.get<TH2>(HIST("hEventCount"))->GetYaxis()->SetBinLabel(4, "Omega");

    // QA
    // V0 QA
    registry.add("QAhisto/V0/hqaV0radiusbefore", "", {HistType::kTH1D, {{200, 0, 200}}});
    registry.add("QAhisto/V0/hqaV0radiusafter", "", {HistType::kTH1D, {{200, 0, 200}}});
    registry.add("QAhisto/V0/hqaV0cosPAbefore", "", {HistType::kTH1D, {{1000, 0.95, 1}}});
    registry.add("QAhisto/V0/hqaV0cosPAafter", "", {HistType::kTH1D, {{1000, 0.95, 1}}});
    registry.add("QAhisto/V0/hqadcaV0daubefore", "", {HistType::kTH1D, {{100, 0, 1}}});
    registry.add("QAhisto/V0/hqadcaV0dauafter", "", {HistType::kTH1D, {{100, 0, 1}}});
    registry.add("QAhisto/V0/hqaarm_podobefore", "", {HistType::kTH2D, {{100, -1, 1}, {50, 0, 0.3}}});
    registry.add("QAhisto/V0/hqaarm_podoafter", "", {HistType::kTH2D, {{100, -1, 1}, {50, 0, 0.3}}});
    registry.add("QAhisto/V0/hqadcapostoPVbefore", "", {HistType::kTH1D, {{1000, -10, 10}}});
    registry.add("QAhisto/V0/hqadcapostoPVafter", "", {HistType::kTH1D, {{1000, -10, 10}}});
    registry.add("QAhisto/V0/hqadcanegtoPVbefore", "", {HistType::kTH1D, {{1000, -10, 10}}});
    registry.add("QAhisto/V0/hqadcanegtoPVafter", "", {HistType::kTH1D, {{1000, -10, 10}}});
    // Cascade QA
    registry.add("QAhisto/Casc/hqaCasccosPAbefore", "", {HistType::kTH1D, {{1000, 0.95, 1}}});
    registry.add("QAhisto/Casc/hqaCasccosPAafter", "", {HistType::kTH1D, {{1000, 0.95, 1}}});
    registry.add("QAhisto/Casc/hqaCascV0cosPAbefore", "", {HistType::kTH1D, {{1000, 0.95, 1}}});
    registry.add("QAhisto/Casc/hqaCascV0cosPAafter", "", {HistType::kTH1D, {{1000, 0.95, 1}}});
    registry.add("QAhisto/Casc/hqadcaCascV0toPVbefore", "", {HistType::kTH1D, {{1000, -10, 10}}});
    registry.add("QAhisto/Casc/hqadcaCascV0toPVafter", "", {HistType::kTH1D, {{1000, -10, 10}}});
    registry.add("QAhisto/Casc/hqadcaCascBachtoPVbefore", "", {HistType::kTH1D, {{1000, -10, 10}}});
    registry.add("QAhisto/Casc/hqadcaCascBachtoPVafter", "", {HistType::kTH1D, {{1000, -10, 10}}});
    registry.add("QAhisto/Casc/hqadcaCascdaubefore", "", {HistType::kTH1D, {{100, 0, 1}}});
    registry.add("QAhisto/Casc/hqadcaCascdauafter", "", {HistType::kTH1D, {{100, 0, 1}}});
    registry.add("QAhisto/Casc/hqadcaCascV0daubefore", "", {HistType::kTH1D, {{100, 0, 1}}});
    registry.add("QAhisto/Casc/hqadcaCascV0dauafter", "", {HistType::kTH1D, {{100, 0, 1}}});

    // cumulant of flow
    registry.add("c22", ";Centrality  (%) ; C_{2}{2} ", {HistType::kTProfile, {axisMultiplicity}});
    registry.add("c24", ";Centrality  (%) ; C_{2}{2} ", {HistType::kTProfile, {axisMultiplicity}});
    registry.add("K0sc22", ";Centrality  (%) ; C_{2}{2} ", {HistType::kTProfile, {axisMultiplicity}});
    registry.add("Lambdac22", ";Centrality  (%) ; C_{2}{2} ", {HistType::kTProfile, {axisMultiplicity}});
    registry.add("c22dpt", ";Centrality  (%) ; C_{2}{2}", {HistType::kTProfile2D, {cfgaxisPt, axisMultiplicity}});
    registry.add("c24dpt", ";Centrality  (%) ; C_{2}{4}", {HistType::kTProfile2D, {cfgaxisPt, axisMultiplicity}});
    // pt-diff cumulant of flow
    registry.add("Xic22dpt", ";pt ; C_{2}{2} ", {HistType::kTProfile3D, {cfgaxisPtXi, cfgaxisXiMassforflow, axisMultiplicity}});
    registry.add("Omegac22dpt", ";pt ; C_{2}{2} ", {HistType::kTProfile3D, {cfgaxisPtXi, cfgaxisOmegaMassforflow, axisMultiplicity}});
    registry.add("K0sc22dpt", ";pt ; C_{2}{2} ", {HistType::kTProfile3D, {cfgaxisPtV0, cfgaxisK0sMassforflow, axisMultiplicity}});
    registry.add("Lambdac22dpt", ";pt ; C_{2}{2} ", {HistType::kTProfile3D, {cfgaxisPtV0, cfgaxisLambdaMassforflow, axisMultiplicity}});
    registry.add("Xic24dpt", ";pt ; C_{2}{4} ", {HistType::kTProfile3D, {cfgaxisPtXi, cfgaxisXiMassforflow, axisMultiplicity}});
    registry.add("Omegac24dpt", ";pt ; C_{2}{4} ", {HistType::kTProfile3D, {cfgaxisPtXi, cfgaxisOmegaMassforflow, axisMultiplicity}});
    registry.add("K0sc24dpt", ";pt ; C_{2}{4} ", {HistType::kTProfile3D, {cfgaxisPtV0, cfgaxisK0sMassforflow, axisMultiplicity}});
    registry.add("Lambdac24dpt", ";pt ; C_{2}{4} ", {HistType::kTProfile3D, {cfgaxisPtV0, cfgaxisLambdaMassforflow, axisMultiplicity}});
    // for Jackknife
    if (cfgDoJackknife) {
      for (int i = 1; i <= nPtBins; i++) {
        refc22[i - 1] = registry.add<TProfile>(Form("Jackknife/REF/c22_%d", i), ";Centrality  (%) ; C_{2}{2} ", {HistType::kTProfile, {axisMultiplicity}});
        refc24[i - 1] = registry.add<TProfile>(Form("Jackknife/REF/c24_%d", i), ";Centrality  (%) ; C_{2}{2} ", {HistType::kTProfile, {axisMultiplicity}});
        xic22[i - 1] = registry.add<TProfile3D>(Form("Jackknife/Xi/Xic22dpt_%d", i), ";pt ; C_{2}{2} ", {HistType::kTProfile3D, {cfgaxisPtXi, cfgaxisXiMassforflow, axisMultiplicity}});
        omegac22[i - 1] = registry.add<TProfile3D>(Form("Jackknife/Omega/Omegac22dpt_%d", i), ";pt ; C_{2}{2} ", {HistType::kTProfile3D, {cfgaxisPtXi, cfgaxisOmegaMassforflow, axisMultiplicity}});
        k0sc22[i - 1] = registry.add<TProfile3D>(Form("Jackknife/K0s/K0sc22dpt_%d", i), ";pt ; C_{2}{2} ", {HistType::kTProfile3D, {cfgaxisPtV0, cfgaxisK0sMassforflow, axisMultiplicity}});
        lambdac22[i - 1] = registry.add<TProfile3D>(Form("Jackknife/Lambda/Lambdac22dpt_%d", i), ";pt ; C_{2}{2} ", {HistType::kTProfile3D, {cfgaxisPtV0, cfgaxisLambdaMassforflow, axisMultiplicity}});
        xic24[i - 1] = registry.add<TProfile3D>(Form("Jackknife/Xi/Xic24dpt_%d", i), ";pt ; C_{2}{4} ", {HistType::kTProfile3D, {cfgaxisPtXi, cfgaxisXiMassforflow, axisMultiplicity}});
        omegac24[i - 1] = registry.add<TProfile3D>(Form("Jackknife/Omega/Omegac24dpt_%d", i), ";pt ; C_{2}{4} ", {HistType::kTProfile3D, {cfgaxisPtXi, cfgaxisOmegaMassforflow, axisMultiplicity}});
        k0sc24[i - 1] = registry.add<TProfile3D>(Form("Jackknife/K0s/K0sc24dpt_%d", i), ";pt ; C_{2}{4} ", {HistType::kTProfile3D, {cfgaxisPtV0, cfgaxisK0sMassforflow, axisMultiplicity}});
        lambdac24[i - 1] = registry.add<TProfile3D>(Form("Jackknife/Lambda/Lambdac24dpt_%d", i), ";pt ; C_{2}{4} ", {HistType::kTProfile3D, {cfgaxisPtV0, cfgaxisLambdaMassforflow, axisMultiplicity}});
      }
    }
    // MC True flow
    registry.add("MC/c22MC", ";Centrality  (%) ; C_{2}{2} ", {HistType::kTProfile, {axisMultiplicity}});
    registry.add("MC/Xic22dptMC", ";pt ; C_{2}{2} ", {HistType::kTProfile2D, {cfgaxisPtXi, axisMultiplicity}});
    registry.add("MC/Omegac22dptMC", ";pt ; C_{2}{2} ", {HistType::kTProfile2D, {cfgaxisPtXi, axisMultiplicity}});
    registry.add("MC/K0sc22dptMC", ";pt ; C_{2}{2} ", {HistType::kTProfile2D, {cfgaxisPtV0, axisMultiplicity}});
    registry.add("MC/Lambdac22dptMC", ";pt ; C_{2}{2} ", {HistType::kTProfile2D, {cfgaxisPtV0, axisMultiplicity}});
    // InvMass(GeV) of casc and v0
    registry.add("InvMassXi_all", "", {HistType::kTHnSparseF, {cfgaxisPtXi, axisXiMass, cfgaxisEta, axisMultiplicity}});
    registry.add("InvMassOmega_all", "", {HistType::kTHnSparseF, {cfgaxisPtXi, axisOmegaMass, cfgaxisEta, axisMultiplicity}});
    registry.add("InvMassOmega", "", {HistType::kTHnSparseF, {cfgaxisPtXi, axisOmegaMass, cfgaxisEta, axisMultiplicity}});
    registry.add("InvMassXi", "", {HistType::kTHnSparseF, {cfgaxisPtXi, axisXiMass, cfgaxisEta, axisMultiplicity}});
    registry.add("InvMassK0s_all", "", {HistType::kTHnSparseF, {cfgaxisPtV0, axisK0sMass, cfgaxisEta, axisMultiplicity}});
    registry.add("InvMassLambda_all", "", {HistType::kTHnSparseF, {cfgaxisPtV0, axisLambdaMass, cfgaxisEta, axisMultiplicity}});
    registry.add("InvMassK0s", "", {HistType::kTHnSparseF, {cfgaxisPtV0, axisK0sMass, cfgaxisEta, axisMultiplicity}});
    registry.add("InvMassLambda", "", {HistType::kTHnSparseF, {cfgaxisPtV0, axisLambdaMass, cfgaxisEta, axisMultiplicity}});
    // for local density correlation
    registry.add("MC/densityMCGenK0s", "", {HistType::kTH3D, {cfgaxisPtV0, cfgaxisNch, cfgaxisLocalDensity}});
    registry.add("MC/densityMCGenLambda", "", {HistType::kTH3D, {cfgaxisPtV0, cfgaxisNch, cfgaxisLocalDensity}});
    registry.add("MC/densityMCGenXi", "", {HistType::kTH3D, {cfgaxisPtXi, cfgaxisNch, cfgaxisLocalDensity}});
    registry.add("MC/densityMCGenOmega", "", {HistType::kTH3D, {cfgaxisPtXi, cfgaxisNch, cfgaxisLocalDensity}});
    registry.add("MC/densityMCRecK0s", "", {HistType::kTHnSparseF, {cfgaxisPtV0, cfgaxisNch, cfgaxisLocalDensity, axisK0sMass}});
    registry.add("MC/densityMCRecLambda", "", {HistType::kTHnSparseF, {cfgaxisPtV0, cfgaxisNch, cfgaxisLocalDensity, axisLambdaMass}});
    registry.add("MC/densityMCRecXi", "", {HistType::kTHnSparseF, {cfgaxisPtXi, cfgaxisNch, cfgaxisLocalDensity, axisXiMass}});
    registry.add("MC/densityMCRecOmega", "", {HistType::kTHnSparseF, {cfgaxisPtXi, cfgaxisNch, cfgaxisLocalDensity, axisOmegaMass}});

    // Data
    fGFW->AddRegion("reffull", -0.8, 0.8, 1, 1); // ("name", etamin, etamax, ptbinnum, bitmask)eta region -0.8 to 0.8
    fGFW->AddRegion("refN10", -0.8, -0.4, 1, 1);
    fGFW->AddRegion("refP10", 0.4, 0.8, 1, 1);
    // POI
    fGFW->AddRegion("poiN10dpt", -0.8, -0.4, nPtBins, 32);
    fGFW->AddRegion("poiP10dpt", 0.4, 0.8, nPtBins, 32);
    fGFW->AddRegion("poifulldpt", -0.8, 0.8, nPtBins, 32);
    fGFW->AddRegion("poioldpt", -0.8, 0.8, nPtBins, 1);

    int nXiptMassBins = nXiPtBins * cfgmassbins[2];
    fGFW->AddRegion("poiXiPdpt", 0.4, 0.8, nXiptMassBins, 2);
    fGFW->AddRegion("poiXiNdpt", -0.8, -0.4, nXiptMassBins, 2);
    fGFW->AddRegion("poiXifulldpt", -0.8, 0.8, nXiptMassBins, 2);
    fGFW->AddRegion("poiXiP", 0.4, 0.8, 1, 2);
    fGFW->AddRegion("poiXiN", -0.8, -0.4, 1, 2);
    int nOmegaptMassBins = nXiPtBins * cfgmassbins[3];
    fGFW->AddRegion("poiOmegaPdpt", 0.4, 0.8, nOmegaptMassBins, 4);
    fGFW->AddRegion("poiOmegaNdpt", -0.8, -0.4, nOmegaptMassBins, 4);
    fGFW->AddRegion("poiOmegafulldpt", -0.8, 0.8, nOmegaptMassBins, 4);
    fGFW->AddRegion("poiOmegaP", 0.4, 0.8, 1, 4);
    fGFW->AddRegion("poiOmegaN", -0.8, -0.4, 1, 4);
    int nK0sptMassBins = nV0PtBins * cfgmassbins[0];
    fGFW->AddRegion("poiK0sPdpt", 0.4, 0.8, nK0sptMassBins, 8);
    fGFW->AddRegion("poiK0sNdpt", -0.8, -0.4, nK0sptMassBins, 8);
    fGFW->AddRegion("poiK0sfulldpt", -0.8, 0.8, nK0sptMassBins, 8);
    fGFW->AddRegion("poiK0sP", 0.4, 0.8, 1, 8);
    fGFW->AddRegion("poiK0sN", -0.8, 0.4, 1, 8);
    int nLambdaptMassBins = nV0PtBins * cfgmassbins[1];
    fGFW->AddRegion("poiLambdaPdpt", 0.4, 0.8, nLambdaptMassBins, 16);
    fGFW->AddRegion("poiLambdaNdpt", -0.8, -0.4, nLambdaptMassBins, 16);
    fGFW->AddRegion("poiLambdafulldpt", -0.8, 0.8, nLambdaptMassBins, 16);
    fGFW->AddRegion("poiLambdaP", 0.4, 0.8, 1, 16);
    fGFW->AddRegion("poiLambdaN", -0.8, -0.4, 1, 16);
    // MC
    fGFW->AddRegion("refN10MC", -0.8, -0.4, 1, 64);
    fGFW->AddRegion("refP10MC", 0.4, 0.8, 1, 64);
    fGFW->AddRegion("poiXiPdptMC", 0.4, 0.8, nXiptMassBins, 128);
    fGFW->AddRegion("poiXiNdptMC", -0.8, -0.4, nXiptMassBins, 128);
    fGFW->AddRegion("poiOmegaPdptMC", 0.4, 0.8, nOmegaptMassBins, 256);
    fGFW->AddRegion("poiOmegaNdptMC", -0.8, -0.4, nOmegaptMassBins, 256);
    fGFW->AddRegion("poiK0sPdptMC", 0.4, 0.8, nK0sptMassBins, 512);
    fGFW->AddRegion("poiK0sNdptMC", -0.8, -0.4, nK0sptMassBins, 512);
    fGFW->AddRegion("poiLambdaPdptMC", 0.4, 0.8, nLambdaptMassBins, 1024);
    fGFW->AddRegion("poiLambdaNdptMC", -0.8, -0.4, nLambdaptMassBins, 1024);
    // pushback
    // Data
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiP10dpt {2} refN10 {-2}", "Poi10Gap22dpta", kTRUE)); // 0
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiN10dpt {2} refP10 {-2}", "Poi10Gap22dptb", kTRUE));
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poifulldpt reffull | poioldpt {2 2 -2 -2}", "Poi10Gap24dpt", kTRUE));
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiXiPdpt {2} refN10 {-2}", "Xi10Gap22a", kTRUE));
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiXiNdpt {2} refP10 {-2}", "Xi10Gap22b", kTRUE));
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiXifulldpt reffull {2 2 -2 -2}", "Xi10Gap24", kTRUE)); // 5
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiOmegaPdpt {2} refN10 {-2}", "Omega10Gap22a", kTRUE));
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiOmegaNdpt {2} refP10 {-2}", "Omega10Gap22b", kTRUE));
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiOmegafulldpt reffull {2 2 -2 -2}", "Xi10Gap24", kTRUE));
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiK0sPdpt {2} refN10 {-2}", "K0short10Gap22a", kTRUE));
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiK0sNdpt {2} refP10 {-2}", "K0short10Gap22b", kTRUE)); // 10
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiK0sfulldpt reffull {2 2 -2 -2}", "Xi10Gap24", kTRUE));
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiLambdaPdpt {2} refN10 {-2}", "Lambda10Gap22a", kTRUE));
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiLambdaNdpt {2} refP10 {-2}", "Lambda10Gap22b", kTRUE));
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiLambdafulldpt reffull {2 2 -2 -2}", "Xi10Gap24a", kTRUE));
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("refP10 {2} refN10 {-2}", "Ref10Gap22a", kFALSE)); // 15
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("reffull reffull {2 2 -2 -2}", "Ref10Gap24", kFALSE));
    // MC
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiXiPdptMC {2} refN10MC {-2}", "MCXi10Gap22a", kTRUE)); // 17
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiXiNdptMC {2} refP10MC {-2}", "MCXi10Gap22b", kTRUE));
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiOmegaPdptMC {2} refN10MC {-2}", "MCOmega10Gap22a", kTRUE));
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiOmegaNdptMC {2} refP10MC {-2}", "MCOmega10Gap22b", kTRUE)); // 20
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiK0sPdptMC {2} refN10MC {-2}", "MCK0s10Gap22a", kTRUE));
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiK0sNdptMC {2} refP10MC {-2}", "MCK0s10Gap22b", kTRUE));
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiLambdaPdptMC {2} refN10MC {-2}", "MCLambda10Gap22a", kTRUE));
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("poiLambdaNdptMC {2} refP10MC {-2}", "MCLambda10Gap22b", kTRUE));
    corrconfigs.push_back(fGFW->GetCorrelatorConfig("refP10MC {2} refN10MC {-2}", "MCRef10Gap22a", kFALSE)); // 25
    fGFW->CreateRegions(); // finalize the initialization

    // used for event selection
    fMultPVCutLow = new TF1("fMultPVCutLow", "[0]+[1]*x+[2]*x*x+[3]*x*x*x+[4]*x*x*x*x - 3.5*([5]+[6must ]*x+[7]*x*x+[8]*x*x*x+[9]*x*x*x*x)", 0, 100);
    fMultPVCutLow->SetParameters(3257.29, -121.848, 1.98492, -0.0172128, 6.47528e-05, 154.756, -1.86072, -0.0274713, 0.000633499, -3.37757e-06);
    fMultPVCutHigh = new TF1("fMultPVCutHigh", "[0]+[1]*x+[2]*x*x+[3]*x*x*x+[4]*x*x*x*x + 3.5*([5]+[6]*x+[7]*x*x+[8]*x*x*x+[9]*x*x*x*x)", 0, 100);
    fMultPVCutHigh->SetParameters(3257.29, -121.848, 1.98492, -0.0172128, 6.47528e-05, 154.756, -1.86072, -0.0274713, 0.000633499, -3.37757e-06);
    fT0AV0AMean = new TF1("fT0AV0AMean", "[0]+[1]*x", 0, 200000);
    fT0AV0AMean->SetParameters(-1601.0581, 9.417652e-01);
    fT0AV0ASigma = new TF1("fT0AV0ASigma", "[0]+[1]*x+[2]*x*x+[3]*x*x*x+[4]*x*x*x*x", 0, 200000);
    fT0AV0ASigma->SetParameters(463.4144, 6.796509e-02, -9.097136e-07, 7.971088e-12, -2.600581e-17);

    // fWeight output
    if (cfgOutputNUAWeights) {
      fWeightsREF->setPtBins(nPtBins, &(axisPt.binEdges)[0]);
      fWeightsREF->init(true, false);
      fWeightsK0s->setPtBins(nPtBins, &(axisPt.binEdges)[0]);
      fWeightsK0s->init(true, false);
      fWeightsLambda->setPtBins(nPtBins, &(axisPt.binEdges)[0]);
      fWeightsLambda->init(true, false);
      fWeightsXi->setPtBins(nPtBins, &(axisPt.binEdges)[0]);
      fWeightsXi->init(true, false);
      fWeightsOmega->setPtBins(nPtBins, &(axisPt.binEdges)[0]);
      fWeightsOmega->init(true, false);
    }
  }

  // input HIST("name")
  template <char... chars>
  void fillProfile(const GFW::CorrConfig& corrconf, const ConstStr<chars...>& tarName, const double& cent)
  {
    double dnx, val;
    dnx = fGFW->Calculate(corrconf, 0, kTRUE).real();
    if (dnx == 0)
      return;
    if (!corrconf.pTDif) {
      val = fGFW->Calculate(corrconf, 0, kFALSE).real() / dnx;
      if (std::fabs(val) < 1)
        registry.fill(tarName, cent, val, dnx);
      return;
    }
    return;
  }

  // input shared_ptr<TProfile>
  void fillProfile(const GFW::CorrConfig& corrconf, std::shared_ptr<TProfile> TProfile, const double& cent)
  {
    double dnx, val;
    dnx = fGFW->Calculate(corrconf, 0, kTRUE).real();
    if (dnx == 0)
      return;
    if (!corrconf.pTDif) {
      val = fGFW->Calculate(corrconf, 0, kFALSE).real() / dnx;
      if (std::fabs(val) < 1)
        TProfile->Fill(cent, val, dnx);
      return;
    }
    return;
  }

  template <char... chars>
  void fillProfilepT(const GFW::CorrConfig& corrconf, const ConstStr<chars...>& tarName, const int& ptbin, const double& cent)
  {
    float dnx = 0;
    float val = 0;
    dnx = fGFW->Calculate(corrconf, ptbin - 1, kTRUE).real();
    if (dnx == 0)
      return;
    val = fGFW->Calculate(corrconf, ptbin - 1, kFALSE).real() / dnx;
    if (std::fabs(val) < 1) {
      registry.fill(tarName, fPtAxis->GetBinCenter(ptbin), cent, val, dnx);
    }
    return;
  }

  template <char... chars>
  void fillProfilepTMC(const GFW::CorrConfig& corrconf, const ConstStr<chars...>& tarName, const int& ptbin, const int& PDGCode, const double& cent)
  {
    TAxis* fpt = nullptr;
    if (PDGCode == kXiMinus) {
      fpt = fXiPtAxis;
    } else if (PDGCode == kOmegaMinus) {
      fpt = fXiPtAxis;
    } else if (PDGCode == kK0Short) {
      fpt = fV0PtAxis;
    } else if (PDGCode == kLambda0) {
      fpt = fV0PtAxis;
    } else {
      LOGF(error, "Error, please put in correct PDGCode of K0s, Lambda, Xi or Omega");
      return;
    }
    float dnx = 0;
    float val = 0;
    dnx = fGFW->Calculate(corrconf, ptbin - 1, kTRUE).real();
    if (dnx == 0)
      return;
    val = fGFW->Calculate(corrconf, ptbin - 1, kFALSE).real() / dnx;
    if (std::fabs(val) < 1) {
      registry.fill(tarName, fpt->GetBinCenter(ptbin), cent, val, dnx);
    }
    return;
  }

  // input HIST("name")
  template <char... chars>
  void fillProfilepTMass(const GFW::CorrConfig& corrconf, const ConstStr<chars...>& tarName, const int& ptbin, const int& PDGCode, const float& cent)
  {
    int nMassBins = 0;
    int nptbins = 0;
    TAxis* fMass = nullptr;
    TAxis* fpt = nullptr;
    if (PDGCode == kXiMinus) {
      nMassBins = cfgmassbins[2];
      nptbins = nXiPtBins;
      fpt = fXiPtAxis;
      fMass = fXiMass;
    } else if (PDGCode == kOmegaMinus) {
      nMassBins = cfgmassbins[3];
      nptbins = nXiPtBins;
      fpt = fXiPtAxis;
      fMass = fOmegaMass;
    } else if (PDGCode == kK0Short) {
      nMassBins = cfgmassbins[0];
      nptbins = nV0PtBins;
      fpt = fV0PtAxis;
      fMass = fK0sMass;
    } else if (PDGCode == kLambda0) {
      nMassBins = cfgmassbins[1];
      nptbins = nV0PtBins;
      fpt = fV0PtAxis;
      fMass = fLambdaMass;
    } else {
      LOGF(error, "Error, please put in correct PDGCode of K0s, Lambda, Xi or Omega");
      return;
    }
    for (int massbin = 1; massbin <= nMassBins; massbin++) {
      float dnx = 0;
      float val = 0;
      dnx = fGFW->Calculate(corrconf, (ptbin - 1) + ((massbin - 1) * nptbins), kTRUE).real();
      if (dnx == 0)
        continue;
      val = fGFW->Calculate(corrconf, (ptbin - 1) + ((massbin - 1) * nptbins), kFALSE).real() / dnx;
      if (std::fabs(val) < 1) {
        registry.fill(tarName, fpt->GetBinCenter(ptbin), fMass->GetBinCenter(massbin), cent, val, dnx);
      }
    }
    return;
  }

  // input shared_ptr<TProfile3D>
  void fillProfilepTMass(const GFW::CorrConfig& corrconf, std::shared_ptr<TProfile3D> TProfile3D, const int& ptbin, const int& PDGCode, const float& cent)
  {
    int nMassBins = 0;
    int nptbins = 0;
    TAxis* fMass = nullptr;
    TAxis* fpt = nullptr;
    if (PDGCode == kXiMinus) {
      nMassBins = cfgmassbins[2];
      nptbins = nXiPtBins;
      fpt = fXiPtAxis;
      fMass = fXiMass;
    } else if (PDGCode == kOmegaMinus) {
      nMassBins = cfgmassbins[3];
      nptbins = nXiPtBins;
      fpt = fXiPtAxis;
      fMass = fOmegaMass;
    } else if (PDGCode == kK0Short) {
      nMassBins = cfgmassbins[0];
      nptbins = nV0PtBins;
      fpt = fV0PtAxis;
      fMass = fK0sMass;
    } else if (PDGCode == kLambda0) {
      nMassBins = cfgmassbins[1];
      nptbins = nV0PtBins;
      fpt = fV0PtAxis;
      fMass = fLambdaMass;
    } else {
      LOGF(error, "Error, please put in correct PDGCode of K0s, Lambda, Xi or Omega");
      return;
    }
    for (int massbin = 1; massbin <= nMassBins; massbin++) {
      float dnx = 0;
      float val = 0;
      dnx = fGFW->Calculate(corrconf, (ptbin - 1) + ((massbin - 1) * nptbins), kTRUE).real();
      if (dnx == 0)
        continue;
      val = fGFW->Calculate(corrconf, (ptbin - 1) + ((massbin - 1) * nptbins), kFALSE).real() / dnx;
      if (std::fabs(val) < 1) {
        TProfile3D->Fill(fpt->GetBinCenter(ptbin), fMass->GetBinCenter(massbin), cent, val, dnx);
      }
    }
    return;
  }

  void loadCorrections(uint64_t timestamp)
  {
    if (correctionsLoaded)
      return;
    if (cfgAcceptance.size() == 5) {
      for (int i = 0; i <= 4; i++) {
        mAcceptance.push_back(ccdb->getForTimeStamp<GFWWeights>(cfgAcceptance[i], timestamp));
      }
      if (mAcceptance.size() == 5)
        LOGF(info, "Loaded acceptance weights");
      else
        LOGF(warning, "Could not load acceptance weights");
    }
    if (cfgEfficiency.size() == 5) {
      for (int i = 0; i <= 4; i++) {
        mEfficiency.push_back(ccdb->getForTimeStamp<TH1D>(cfgEfficiency[i], timestamp));
      }
      if (mEfficiency.size() == 5)
        LOGF(info, "Loaded efficiency histogram");
      else
        LOGF(fatal, "Could not load efficiency histogram");
    }
    correctionsLoaded = true;
  }

  template <typename TrackObject>
  bool setCurrentParticleWeights(float& weight_nue, float& weight_nua, TrackObject track, float vtxz, int ispecies)
  {
    float eff = 1.;
    if (mEfficiency.size() == 5)
      eff = mEfficiency[ispecies]->GetBinContent(mEfficiency[ispecies]->FindBin(track.pt()));
    else
      eff = 1.0;
    if (eff == 0)
      return false;
    weight_nue = 1. / eff;
    if (mAcceptance.size() == 5)
      weight_nua = mAcceptance[ispecies]->getNUA(track.phi(), track.eta(), vtxz);
    else
      weight_nua = 1;
    return true;
  }

  template <typename TrackObject>
  bool setCurrentLocalDensityWeights(float& weight_loc, TrackObject track, double locDensity, int ispecies)
  {
    auto cfgLocDenPara = (std::vector<std::vector<double>>){cfgLocDenParaK0s, cfgLocDenParaLambda, cfgLocDenParaXi, cfgLocDenParaOmega};
    int ptbin = fXiPtAxis->FindBin(track.pt());
    if (ptbin == 0 || ptbin == (fXiPtAxis->GetNbins() + 1)) {
      weight_loc = 1.0;
      return true;
    }
    double paraA = cfgLocDenPara[ispecies - 1][2 * ptbin - 2];
    double paraB = cfgLocDenPara[ispecies - 1][2 * ptbin - 1];
    double density = locDensity * 200 / (2 * cfgDeltaPhiLocDen + 1);
    double eff = std::exp(paraA * density + paraB);
    weight_loc = 1 / eff;
    return true;
  }
  // event selection
  template <typename TCollision>
  bool eventSelected(TCollision collision, const float centrality)
  {
    if (collision.alias_bit(kTVXinTRD)) {
      // TRD triggered
      return false;
    }
    if (!collision.selection_bit(o2::aod::evsel::kNoTimeFrameBorder)) {
      // reject collisions close to Time Frame borders
      // https://its.cern.ch/jira/browse/O2-4623
      return false;
    }
    if (!collision.selection_bit(o2::aod::evsel::kNoITSROFrameBorder)) {
      // reject events affected by the ITS ROF border
      // https://its.cern.ch/jira/browse/O2-4309
      return false;
    }
    if (!collision.selection_bit(o2::aod::evsel::kNoSameBunchPileup)) {
      // rejects collisions which are associated with the same "found-by-T0" bunch crossing
      // https://indico.cern.ch/event/1396220/#1-event-selection-with-its-rof
      return false;
    }
    if (!collision.selection_bit(o2::aod::evsel::kIsGoodZvtxFT0vsPV)) {
      // removes collisions with large differences between z of PV by tracks and z of PV from FT0 A-C time difference
      // use this cut at low multiplicities with caution
      return false;
    }
    if (!collision.selection_bit(o2::aod::evsel::kNoCollInTimeRangeStandard)) {
      // no collisions in specified time range
      return 0;
    }
    if (!collision.selection_bit(o2::aod::evsel::kIsGoodITSLayersAll)) {
      // cut time intervals with dead ITS staves
      return 0;
    }
    float vtxz = -999;
    if (collision.numContrib() > 1) {
      vtxz = collision.posZ();
      float zRes = std::sqrt(collision.covZZ());
      if (zRes > 0.25 && collision.numContrib() < 20)
        vtxz = -999;
    }
    auto multNTracksPV = collision.multNTracksPV();
    auto occupancy = collision.trackOccupancyInTimeRange();

    if (std::fabs(vtxz) > cfgCutVertex)
      return false;
    if (multNTracksPV < fMultPVCutLow->Eval(centrality))
      return false;
    if (multNTracksPV > fMultPVCutHigh->Eval(centrality))
      return false;
    if (occupancy > cfgCutOccupancyHigh)
      return 0;

    // V0A T0A 5 sigma cut
    if (std::fabs(collision.multFV0A() - fT0AV0AMean->Eval(collision.multFT0A())) > 5 * fT0AV0ASigma->Eval(collision.multFT0A()))
      return 0;

    return true;
  }

  void processData(AodCollisions::iterator const& collision, aod::BCsWithTimestamps const&, AodTracks const& tracks, aod::CascDataExt const& Cascades, aod::V0Datas const& V0s, DaughterTracks const&)
  {
    int nTot = tracks.size();
    int candNumAll[4] = {0, 0, 0, 0};
    int candNum[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; i++) {
      registry.fill(HIST("hEventCount"), 0.5, i + 0.5);
    }
    if (nTot < 1)
      return;
    fGFW->Clear();
    const auto cent = collision.centFT0C();
    if (!collision.sel8())
      return;
    if (eventSelected(collision, cent))
      return;
    TH1D* hLocalDensity = new TH1D("hphi", "hphi", 400, -constants::math::TwoPI, constants::math::TwoPI);
    auto bc = collision.bc_as<aod::BCsWithTimestamps>();
    loadCorrections(bc.timestamp());
    float vtxz = collision.posZ();
    registry.fill(HIST("hVtxZ"), vtxz);
    registry.fill(HIST("hMult"), nTot);
    registry.fill(HIST("hCent"), cent);
    for (int i = 0; i < 4; i++) {
      registry.fill(HIST("hEventCount"), 1.5, i + 0.5);
    }

    float weff = 1;
    float wacc = 1;
    float wloc = 1;
    double nch = 0;
    // fill GFW ref flow
    for (const auto& track : tracks) {
      if (cfgDoAccEffCorr) {
        if (!setCurrentParticleWeights(weff, wacc, track, vtxz, 0))
          continue;
      }
      registry.fill(HIST("hPhi"), track.phi());
      registry.fill(HIST("hPhicorr"), track.phi(), wacc);
      registry.fill(HIST("hEta"), track.eta());
      registry.fill(HIST("hEtaPhiVtxzREF"), track.phi(), track.eta(), vtxz, wacc);
      registry.fill(HIST("hPt"), track.pt());
      int ptbin = fPtAxis->FindBin(track.pt()) - 1;
      if ((track.pt() > cfgCutPtMin) && (track.pt() < cfgCutPtMax)) {
        fGFW->Fill(track.eta(), ptbin, track.phi(), wacc * weff, 1); //(eta, ptbin, phi, wacc*weff, bitmask)
      }
      if ((track.pt() > cfgCutPtPOIMin) && (track.pt() < cfgCutPtPOIMax)) {
        fGFW->Fill(track.eta(), ptbin, track.phi(), wacc * weff, 32);
        if (cfgDoLocDenCorr) {
          hLocalDensity->Fill(track.phi(), wacc * weff);
          hLocalDensity->Fill(RecoDecay::constrainAngle(track.phi(), -constants::math::TwoPI), wacc * weff);
          nch++;
        }
      }
      if (cfgOutputNUAWeights)
        fWeightsREF->fill(track.phi(), track.eta(), vtxz, track.pt(), cent, 0);
    }
    if (cfgDoLocDenCorr) {
      registry.fill(HIST("hCentvsNch"), cent, nch);
    }
    // fill GFW of V0 flow
    for (const auto& v0 : V0s) {
      auto v0posdau = v0.posTrack_as<DaughterTracks>();
      auto v0negdau = v0.negTrack_as<DaughterTracks>();
      // check tpc
      bool isK0s = false;
      bool isLambda = false;
      // fill QA
      registry.fill(HIST("QAhisto/V0/hqaarm_podobefore"), v0.alpha(), v0.qtarm());
      // check daughter TPC and TOF
      // K0short
      if (v0.qtarm() / std::fabs(v0.alpha()) > cfgv0_ArmPodocut && std::fabs(v0.mK0Short() - o2::constants::physics::MassK0Short) < cfgv0_mk0swindow &&
          (!cfgcheckDauTPC || (std::fabs(v0posdau.tpcNSigmaPi()) < cfgNSigma[0] && std::fabs(v0negdau.tpcNSigmaPi()) < cfgNSigma[0])) &&
          (!cfgcheckDauTOF || ((std::fabs(v0posdau.tofNSigmaPi()) < cfgNSigma[3] || v0posdau.pt() < 0.4) && (std::fabs(v0negdau.tofNSigmaPi()) < cfgNSigma[3] || v0negdau.pt() < 0.4)))) {
        registry.fill(HIST("InvMassK0s_all"), v0.pt(), v0.mK0Short(), v0.eta(), cent);
        isK0s = true;
        candNumAll[0] = candNumAll[0] + 1;
        registry.fill(HIST("QAhisto/V0/hqaarm_podoafter"), v0.alpha(), v0.qtarm());
      }
      // Lambda and antiLambda
      if (std::fabs(v0.mLambda() - o2::constants::physics::MassLambda) < cfgv0_mlambdawindow &&
          (!cfgcheckDauTPC || (std::fabs(v0posdau.tpcNSigmaPr()) < cfgNSigma[1] && std::fabs(v0negdau.tpcNSigmaPi()) < cfgNSigma[0])) &&
          (!cfgcheckDauTOF || ((std::fabs(v0posdau.tofNSigmaPr()) < cfgNSigma[4] || v0posdau.pt() < 0.4) && (std::fabs(v0negdau.tofNSigmaPi()) < cfgNSigma[3] || v0negdau.pt() < 0.4)))) {
        registry.fill(HIST("InvMassLambda_all"), v0.pt(), v0.mLambda(), v0.eta(), cent);
        isLambda = true;
        candNumAll[1] = candNumAll[1] + 1;
      } else if (std::fabs(v0.mLambda() - o2::constants::physics::MassLambda) < cfgv0_mlambdawindow &&
                 (!cfgcheckDauTPC || (std::fabs(v0negdau.tpcNSigmaPr()) < cfgNSigma[1] && std::fabs(v0posdau.tpcNSigmaPi()) < cfgNSigma[0])) &&
                 (!cfgcheckDauTOF || ((std::fabs(v0negdau.tofNSigmaPr()) < cfgNSigma[4] || v0negdau.pt() < 0.4) && (std::fabs(v0posdau.tofNSigmaPi()) < cfgNSigma[3] || v0posdau.pt() < 0.4)))) {
        registry.fill(HIST("InvMassLambda_all"), v0.pt(), v0.mLambda(), v0.eta(), cent);
        isLambda = true;
        candNumAll[1] = candNumAll[1] + 1;
      }
      // fill QA before cut
      registry.fill(HIST("QAhisto/V0/hqaV0radiusbefore"), v0.v0radius());
      registry.fill(HIST("QAhisto/V0/hqaV0cosPAbefore"), v0.v0cosPA());
      registry.fill(HIST("QAhisto/V0/hqadcaV0daubefore"), v0.dcaV0daughters());
      registry.fill(HIST("QAhisto/V0/hqadcapostoPVbefore"), v0.dcapostopv());
      registry.fill(HIST("QAhisto/V0/hqadcanegtoPVbefore"), v0.dcanegtopv());
      if (!isK0s && !isLambda)
        continue;
      // track quality check
      if (v0posdau.tpcNClsFound() < cfgtpcclusters)
        continue;
      if (v0negdau.tpcNClsFound() < cfgtpcclusters)
        continue;
      if (v0posdau.tpcNClsFindable() < cfgtpcclufindable)
        continue;
      if (v0negdau.tpcNClsFindable() < cfgtpcclufindable)
        continue;
      if (v0posdau.tpcCrossedRowsOverFindableCls() < cfgtpccrossoverfindable)
        continue;
      if (v0posdau.itsNCls() < cfgitsclusters)
        continue;
      if (v0negdau.itsNCls() < cfgitsclusters)
        continue;
      // topological cut
      if (v0.v0radius() < cfgv0_radius)
        continue;
      if (v0.v0cosPA() < cfgv0_v0cospa)
        continue;
      if (v0.dcaV0daughters() > cfgv0_dcav0dau)
        continue;
      if (std::fabs(v0.dcapostopv()) < cfgv0_dcadautopv)
        continue;
      if (std::fabs(v0.dcanegtopv()) < cfgv0_dcadautopv)
        continue;
      // fill QA after cut
      registry.fill(HIST("QAhisto/V0/hqaV0radiusafter"), v0.v0radius());
      registry.fill(HIST("QAhisto/V0/hqaV0cosPAafter"), v0.v0cosPA());
      registry.fill(HIST("QAhisto/V0/hqadcaV0dauafter"), v0.dcaV0daughters());
      registry.fill(HIST("QAhisto/V0/hqadcapostoPVafter"), v0.dcapostopv());
      registry.fill(HIST("QAhisto/V0/hqadcanegtoPVafter"), v0.dcanegtopv());
      if (isK0s) {
        if (cfgDoAccEffCorr)
          setCurrentParticleWeights(weff, wacc, v0, vtxz, 1);
        if (cfgDoLocDenCorr) {
          int phibin = -999;
          phibin = hLocalDensity->FindBin(RecoDecay::constrainAngle(v0.phi(), -constants::math::PI));
          if (phibin > -900) {
            double density = hLocalDensity->Integral(phibin - cfgDeltaPhiLocDen, phibin + cfgDeltaPhiLocDen);
            setCurrentLocalDensityWeights(wloc, v0, density, 1);
            if (cfgOutputLocDenWeights)
              registry.fill(HIST("MC/densityMCRecK0s"), v0.pt(), nch, density, v0.mK0Short());
          }
        }
        candNum[0] = candNum[0] + 1;
        registry.fill(HIST("InvMassK0s"), v0.pt(), v0.mK0Short(), v0.eta(), cent);
        registry.fill(HIST("hEtaPhiVtxzPOIK0s"), v0.phi(), v0.eta(), vtxz, wacc);
        fGFW->Fill(v0.eta(), fV0PtAxis->FindBin(v0.pt()) - 1 + ((fK0sMass->FindBin(v0.mK0Short()) - 1) * nV0PtBins), v0.phi(), wacc * weff * wloc, 8);
        if (cfgOutputNUAWeights)
          fWeightsK0s->fill(v0.phi(), v0.eta(), vtxz, v0.pt(), cent, 0);
      }
      if (isLambda) {
        if (cfgDoAccEffCorr)
          setCurrentParticleWeights(weff, wacc, v0, vtxz, 2);
        if (cfgDoLocDenCorr) {
          int phibin = -999;
          phibin = hLocalDensity->FindBin(RecoDecay::constrainAngle(v0.phi(), -constants::math::PI));
          if (phibin > -900) {
            double density = hLocalDensity->Integral(phibin - cfgDeltaPhiLocDen, phibin + cfgDeltaPhiLocDen);
            setCurrentLocalDensityWeights(wloc, v0, density, 2);
            if (cfgOutputLocDenWeights)
              registry.fill(HIST("MC/densityMCRecLambda"), v0.pt(), nch, density, v0.mLambda());
          }
        }
        candNum[1] = candNum[1] + 1;
        registry.fill(HIST("InvMassLambda"), v0.pt(), v0.mLambda(), v0.eta(), cent);
        registry.fill(HIST("hEtaPhiVtxzPOILambda"), v0.phi(), v0.eta(), vtxz, wacc);
        fGFW->Fill(v0.eta(), fV0PtAxis->FindBin(v0.pt()) - 1 + ((fLambdaMass->FindBin(v0.mLambda()) - 1) * nV0PtBins), v0.phi(), wacc * weff * wloc, 16);
        if (cfgOutputNUAWeights)
          fWeightsLambda->fill(v0.phi(), v0.eta(), vtxz, v0.pt(), cent, 0);
      }
    }
    // fill GFW of casc flow
    for (const auto& casc : Cascades) {
      auto bachelor = casc.bachelor_as<DaughterTracks>();
      auto posdau = casc.posTrack_as<DaughterTracks>();
      auto negdau = casc.negTrack_as<DaughterTracks>();
      // check TPC
      if (cfgcheckDauTPC && (!posdau.hasTPC() || !negdau.hasTPC() || !bachelor.hasTPC())) {
        continue;
      }
      bool isOmega = false;
      bool isXi = false;
      // Omega and antiOmega
      if (casc.sign() < 0 && (casc.mOmega() > 1.63) && (casc.mOmega() < 1.71) && std::fabs(casc.yOmega()) < cfgCasc_rapidity &&
          (!cfgcheckDauTPC || (std::fabs(bachelor.tpcNSigmaKa()) < cfgNSigma[2] && std::fabs(posdau.tpcNSigmaPr()) < cfgNSigma[1] && std::fabs(negdau.tpcNSigmaPi()) < cfgNSigma[0])) &&
          (!cfgcheckDauTOF || ((std::fabs(bachelor.tofNSigmaKa()) < cfgNSigma[5] || bachelor.pt() < 0.4) && (std::fabs(posdau.tofNSigmaPr()) < cfgNSigma[4] || posdau.pt() < 0.4) && (std::fabs(negdau.tofNSigmaPi()) < cfgNSigma[3] || negdau.pt() < 0.4)))) {
        registry.fill(HIST("InvMassOmega_all"), casc.pt(), casc.mOmega(), casc.eta(), cent);
        isOmega = true;
        candNumAll[3] = candNumAll[3] + 1;
      } else if (casc.sign() > 0 && (casc.mOmega() > 1.63) && (casc.mOmega() < 1.71) && std::fabs(casc.yOmega()) < cfgCasc_rapidity &&
                 (!cfgcheckDauTPC || (std::fabs(bachelor.tpcNSigmaKa()) < cfgNSigma[2] && std::fabs(negdau.tpcNSigmaPr()) < cfgNSigma[1] && std::fabs(posdau.tpcNSigmaPi()) < cfgNSigma[0])) &&
                 (!cfgcheckDauTOF || ((std::fabs(bachelor.tofNSigmaKa()) < cfgNSigma[5] || bachelor.pt() < 0.4) && (std::fabs(negdau.tofNSigmaPr()) < cfgNSigma[4] || negdau.pt() < 0.4) && (std::fabs(posdau.tofNSigmaPi()) < cfgNSigma[3] || posdau.pt() < 0.4)))) {
        registry.fill(HIST("InvMassOmega_all"), casc.pt(), casc.mOmega(), casc.eta(), cent);
        isOmega = true;
        candNumAll[3] = candNumAll[3] + 1;
      }
      // Xi and antiXi
      if (casc.sign() < 0 && (casc.mXi() > 1.30) && (casc.mXi() < 1.37) && std::fabs(casc.yXi()) < cfgCasc_rapidity &&
          (!cfgcheckDauTPC || (std::fabs(bachelor.tpcNSigmaPi()) < cfgNSigma[0] && std::fabs(posdau.tpcNSigmaPr()) < cfgNSigma[1] && std::fabs(negdau.tpcNSigmaPi()) < cfgNSigma[0])) &&
          (!cfgcheckDauTOF || ((std::fabs(bachelor.tofNSigmaPi()) < cfgNSigma[3] || bachelor.pt() < 0.4) && (std::fabs(posdau.tofNSigmaPr()) < cfgNSigma[4] || posdau.pt() < 0.4) && (std::fabs(negdau.tofNSigmaPi()) < cfgNSigma[3] || negdau.pt() < 0.4)))) {
        registry.fill(HIST("InvMassXi_all"), casc.pt(), casc.mXi(), casc.eta(), cent);
        isXi = true;
        candNumAll[2] = candNumAll[2] + 1;
      } else if (casc.sign() > 0 && (casc.mXi() > 1.30) && (casc.mXi() < 1.37) && std::fabs(casc.yXi()) < cfgCasc_rapidity &&
                 (!cfgcheckDauTPC || (std::fabs(bachelor.tpcNSigmaPi()) < cfgNSigma[0] && std::fabs(negdau.tpcNSigmaPr()) < cfgNSigma[1] && std::fabs(posdau.tpcNSigmaPi()) < cfgNSigma[0])) &&
                 (!cfgcheckDauTOF || ((std::fabs(bachelor.tofNSigmaPi()) < cfgNSigma[3] || bachelor.pt() < 0.4) && (std::fabs(negdau.tofNSigmaPr()) < cfgNSigma[4] || negdau.pt() < 0.4) && (std::fabs(posdau.tofNSigmaPi()) < cfgNSigma[3] || posdau.pt() < 0.4)))) {
        registry.fill(HIST("InvMassXi_all"), casc.pt(), casc.mXi(), casc.eta(), cent);
        isXi = true;
        candNumAll[2] = candNumAll[2] + 1;
      }
      // fill QA
      registry.fill(HIST("QAhisto/Casc/hqaCasccosPAbefore"), casc.casccosPA(collision.posX(), collision.posY(), collision.posZ()));
      registry.fill(HIST("QAhisto/Casc/hqaCascV0cosPAbefore"), casc.v0cosPA(collision.posX(), collision.posY(), collision.posZ()));
      registry.fill(HIST("QAhisto/Casc/hqadcaCascV0toPVbefore"), casc.dcav0topv(collision.posX(), collision.posY(), collision.posZ()));
      registry.fill(HIST("QAhisto/Casc/hqadcaCascBachtoPVbefore"), casc.dcabachtopv());
      registry.fill(HIST("QAhisto/Casc/hqadcaCascdaubefore"), casc.dcacascdaughters());
      registry.fill(HIST("QAhisto/Casc/hqadcaCascV0daubefore"), casc.dcaV0daughters());

      if (!isXi && !isOmega)
        continue;
      // topological cut
      if (casc.cascradius() < cfgcasc_radius)
        continue;
      if (casc.casccosPA(collision.posX(), collision.posY(), collision.posZ()) < cfgcasc_casccospa)
        continue;
      if (casc.v0cosPA(collision.posX(), collision.posY(), collision.posZ()) < cfgcasc_v0cospa)
        continue;
      if (std::fabs(casc.dcav0topv(collision.posX(), collision.posY(), collision.posZ())) < cfgcasc_dcav0topv)
        continue;
      if (std::fabs(casc.dcabachtopv()) < cfgcasc_dcabachtopv)
        continue;
      if (casc.dcacascdaughters() > cfgcasc_dcacascdau)
        continue;
      if (casc.dcaV0daughters() > cfgcasc_dcav0dau)
        continue;
      if (std::fabs(casc.mLambda() - o2::constants::physics::MassLambda0) > cfgcasc_mlambdawindow)
        continue;
      // track quality check
      if (bachelor.tpcNClsFound() < cfgtpcclusters)
        continue;
      if (posdau.tpcNClsFound() < cfgtpcclusters)
        continue;
      if (negdau.tpcNClsFound() < cfgtpcclusters)
        continue;
      if (bachelor.itsNCls() < cfgitsclusters)
        continue;
      if (posdau.itsNCls() < cfgitsclusters)
        continue;
      if (negdau.itsNCls() < cfgitsclusters)
        continue;
      // fill QA
      registry.fill(HIST("QAhisto/Casc/hqaCasccosPAafter"), casc.casccosPA(collision.posX(), collision.posY(), collision.posZ()));
      registry.fill(HIST("QAhisto/Casc/hqaCascV0cosPAafter"), casc.v0cosPA(collision.posX(), collision.posY(), collision.posZ()));
      registry.fill(HIST("QAhisto/Casc/hqadcaCascV0toPVafter"), casc.dcav0topv(collision.posX(), collision.posY(), collision.posZ()));
      registry.fill(HIST("QAhisto/Casc/hqadcaCascBachtoPVafter"), casc.dcabachtopv());
      registry.fill(HIST("QAhisto/Casc/hqadcaCascdauafter"), casc.dcacascdaughters());
      registry.fill(HIST("QAhisto/Casc/hqadcaCascV0dauafter"), casc.dcaV0daughters());

      if (isOmega) {
        if (cfgDoAccEffCorr)
          setCurrentParticleWeights(weff, wacc, casc, vtxz, 4);
        if (cfgDoLocDenCorr) {
          int phibin = -999;
          phibin = hLocalDensity->FindBin(RecoDecay::constrainAngle(casc.phi(), -constants::math::PI));
          if (phibin > -900) {
            double density = hLocalDensity->Integral(phibin - cfgDeltaPhiLocDen, phibin + cfgDeltaPhiLocDen);
            setCurrentLocalDensityWeights(wloc, casc, density, 4);
            if (cfgOutputLocDenWeights)
              registry.fill(HIST("MC/densityMCRecOmega"), casc.pt(), nch, density, casc.mOmega());
          }
        }
        candNum[3] = candNum[3] + 1;
        registry.fill(HIST("hEtaPhiVtxzPOIOmega"), casc.phi(), casc.eta(), vtxz, wacc);
        registry.fill(HIST("InvMassOmega"), casc.pt(), casc.mOmega(), casc.eta(), cent);
        fGFW->Fill(casc.eta(), fXiPtAxis->FindBin(casc.pt()) - 1 + ((fOmegaMass->FindBin(casc.mOmega()) - 1) * nXiPtBins), casc.phi(), wacc * weff * wloc, 4);
        if (cfgOutputNUAWeights)
          fWeightsOmega->fill(casc.phi(), casc.eta(), vtxz, casc.pt(), cent, 0);
      }
      if (isXi) {
        if (cfgDoAccEffCorr)
          setCurrentParticleWeights(weff, wacc, casc, vtxz, 3);
        if (cfgDoLocDenCorr) {
          int phibin = -999;
          phibin = hLocalDensity->FindBin(RecoDecay::constrainAngle(casc.phi(), -constants::math::PI));
          if (phibin > -900) {
            double density = hLocalDensity->Integral(phibin - cfgDeltaPhiLocDen, phibin + cfgDeltaPhiLocDen);
            setCurrentLocalDensityWeights(wloc, casc, density, 3);
            if (cfgOutputLocDenWeights)
              registry.fill(HIST("MC/densityMCRecXi"), casc.pt(), nch, density, casc.mXi());
          }
        }
        candNum[2] = candNum[2] + 1;
        registry.fill(HIST("hEtaPhiVtxzPOIXi"), casc.phi(), casc.eta(), vtxz, wacc);
        registry.fill(HIST("InvMassXi"), casc.pt(), casc.mXi(), casc.eta(), cent);
        fGFW->Fill(casc.eta(), fXiPtAxis->FindBin(casc.pt()) - 1 + ((fXiMass->FindBin(casc.mXi()) - 1) * nXiPtBins), casc.phi(), wacc * weff * wloc, 2);
        if (cfgOutputNUAWeights)
          fWeightsXi->fill(casc.phi(), casc.eta(), vtxz, casc.pt(), cent, 0);
      }
    }
    for (int i = 0; i < 4; i++) {
      if (candNumAll[i] > 0) {
        registry.fill(HIST("hEventCount"), 2.5, i + 0.5);
      }
      if (candNum[i] > 0) {
        registry.fill(HIST("hEventCount"), 3.5, i + 0.5);
      }
    }
    delete hLocalDensity;
    // Filling cumulant with ROOT TProfile and loop for all ptBins
    fillProfile(corrconfigs.at(15), HIST("c22"), cent);
    fillProfile(corrconfigs.at(16), HIST("c24"), cent);
    for (int i = 1; i <= nPtBins; i++) {
      fillProfilepT(corrconfigs.at(0), HIST("c22dpt"), i, cent);
      fillProfilepT(corrconfigs.at(1), HIST("c22dpt"), i, cent);
      fillProfilepT(corrconfigs.at(2), HIST("c24dpt"), i, cent);
    }
    for (int i = 1; i <= nV0PtBins; i++) {
      fillProfilepTMass(corrconfigs.at(9), HIST("K0sc22dpt"), i, kK0Short, cent);
      fillProfilepTMass(corrconfigs.at(10), HIST("K0sc22dpt"), i, kK0Short, cent);
      fillProfilepTMass(corrconfigs.at(11), HIST("K0sc24dpt"), i, kK0Short, cent);
      fillProfilepTMass(corrconfigs.at(12), HIST("Lambdac22dpt"), i, kLambda0, cent);
      fillProfilepTMass(corrconfigs.at(13), HIST("Lambdac22dpt"), i, kLambda0, cent);
      fillProfilepTMass(corrconfigs.at(14), HIST("Lambdac24dpt"), i, kLambda0, cent);
    }
    for (int i = 1; i <= nXiPtBins; i++) {
      fillProfilepTMass(corrconfigs.at(3), HIST("Xic22dpt"), i, kXiMinus, cent);
      fillProfilepTMass(corrconfigs.at(4), HIST("Xic22dpt"), i, kXiMinus, cent);
      fillProfilepTMass(corrconfigs.at(5), HIST("Xic24dpt"), i, kXiMinus, cent);
      fillProfilepTMass(corrconfigs.at(6), HIST("Omegac22dpt"), i, kOmegaMinus, cent);
      fillProfilepTMass(corrconfigs.at(7), HIST("Omegac22dpt"), i, kOmegaMinus, cent);
      fillProfilepTMass(corrconfigs.at(8), HIST("Omegac24dpt"), i, kOmegaMinus, cent);
    }
    // Fill subevents flow
    if (cfgDoJackknife) {
      TRandom3* fRdm = new TRandom3(0);
      double eventrdm = 10 * fRdm->Rndm();
      for (int j = 1; j <= 10; j++) {
        if (eventrdm > (j - 1) && eventrdm < j)
          continue;
        fillProfile(corrconfigs.at(15), refc22[j - 1], cent);
        fillProfile(corrconfigs.at(16), refc24[j - 1], cent);
        for (int i = 1; i <= nV0PtBins; i++) {
          fillProfilepTMass(corrconfigs.at(9), k0sc22[j - 1], i, kK0Short, cent);
          fillProfilepTMass(corrconfigs.at(10), k0sc22[j - 1], i, kK0Short, cent);
          fillProfilepTMass(corrconfigs.at(11), k0sc24[j - 1], i, kK0Short, cent);
          fillProfilepTMass(corrconfigs.at(12), lambdac22[j - 1], i, kLambda0, cent);
          fillProfilepTMass(corrconfigs.at(13), lambdac22[j - 1], i, kLambda0, cent);
          fillProfilepTMass(corrconfigs.at(14), lambdac24[j - 1], i, kLambda0, cent);
        }
        for (int i = 1; i <= nXiPtBins; i++) {
          fillProfilepTMass(corrconfigs.at(3), xic22[j - 1], i, kXiMinus, cent);
          fillProfilepTMass(corrconfigs.at(4), xic22[j - 1], i, kXiMinus, cent);
          fillProfilepTMass(corrconfigs.at(5), xic24[j - 1], i, kXiMinus, cent);
          fillProfilepTMass(corrconfigs.at(6), omegac22[j - 1], i, kOmegaMinus, cent);
          fillProfilepTMass(corrconfigs.at(7), omegac22[j - 1], i, kOmegaMinus, cent);
          fillProfilepTMass(corrconfigs.at(8), omegac24[j - 1], i, kOmegaMinus, cent);
        }
      }
    }
  }
  PROCESS_SWITCH(FlowGfwOmegaXi, processData, "", true);

  void processMC(aod::McCollisions::iterator const&, soa::Join<aod::McParticles, aod::ParticlesToTracks> const& tracksGen, soa::SmallGroups<soa::Join<aod::McCollisionLabels, AodCollisions>> const& collisionsRec, AodTracks const&)
  {
    fGFW->Clear();
    int nch = 0;
    double cent = -1;
    TH1D* hLocalDensity = new TH1D("hphi", "hphi", 400, -constants::math::TwoPI, constants::math::TwoPI);
    for (const auto& collision : collisionsRec) {
      if (!collision.sel8())
        return;
      if (eventSelected(collision, cent))
        return;
      cent = collision.centFT0C();
    }
    if (cent < 0)
      return;

    for (auto const& mcParticle : tracksGen) {
      if (!mcParticle.isPhysicalPrimary())
        continue;

      if (mcParticle.has_tracks()) {
        auto const& tracks = mcParticle.tracks_as<AodTracks>();
        for (const auto& track : tracks) {
          if (track.pt() < cfgCutPtPOIMin || track.pt() > cfgCutPtPOIMax)
            continue;
          if (std::fabs(track.eta()) > 0.8)
            continue;
          if (!(track.hasTPC() && track.hasITS()))
            continue;
          if (track.tpcChi2NCl() > cfgCutChi2prTPCcls)
            continue;
          int ptbin = fPtAxis->FindBin(track.pt()) - 1;
          if ((track.pt() > cfgCutPtMin) && (track.pt() < cfgCutPtMax)) {
            fGFW->Fill(track.eta(), ptbin, track.phi(), 1, 64); //(eta, ptbin, phi, wacc*weff, bitmask)
          }
          if ((track.pt() > cfgCutPtPOIMin) && (track.pt() < cfgCutPtPOIMax)) {
            hLocalDensity->Fill(track.phi(), 1);
            hLocalDensity->Fill(RecoDecay::constrainAngle(track.phi(), -constants::math::TwoPI), 1);
            nch++;
          }
        }
      }
    }
    registry.fill(HIST("MC/hCentvsNchMC"), cent, nch);

    for (const auto& cascGen : tracksGen) {
      if (!cascGen.isPhysicalPrimary())
        continue;
      int pdgCode = std::abs(cascGen.pdgCode());
      if (pdgCode != PDG_t::kXiMinus && pdgCode != PDG_t::kOmegaMinus)
        continue;
      if (std::fabs(cascGen.eta()) > 0.8)
        continue;
      if (pdgCode == PDG_t::kXiMinus) {
        int phibin = hLocalDensity->FindBin(RecoDecay::constrainAngle(cascGen.phi(), -constants::math::PI));
        double density = hLocalDensity->Integral(phibin - cfgDeltaPhiLocDen, phibin + cfgDeltaPhiLocDen);
        if (cfgOutputLocDenWeights)
          registry.fill(HIST("MC/densityMCGenXi"), cascGen.pt(), nch, density);
        fGFW->Fill(cascGen.eta(), fXiPtAxis->FindBin(cascGen.pt()) - 1, cascGen.phi(), 1, 128);
      }
      if (pdgCode == PDG_t::kOmegaMinus) {
        int phibin = hLocalDensity->FindBin(RecoDecay::constrainAngle(cascGen.phi(), -constants::math::PI));
        double density = hLocalDensity->Integral(phibin - cfgDeltaPhiLocDen, phibin + cfgDeltaPhiLocDen);
        if (cfgOutputLocDenWeights)
          registry.fill(HIST("MC/densityMCGenOmega"), cascGen.pt(), nch, density);
        fGFW->Fill(cascGen.eta(), fXiPtAxis->FindBin(cascGen.pt()) - 1, cascGen.phi(), 1, 256);
      }
    }
    for (const auto& v0Gen : tracksGen) {
      if (!v0Gen.isPhysicalPrimary())
        continue;
      int pdgCode = std::abs(v0Gen.pdgCode());
      if (pdgCode != PDG_t::kK0Short && pdgCode != PDG_t::kLambda0)
        continue;
      if (std::fabs(v0Gen.eta()) > 0.8)
        continue;
      if (pdgCode == PDG_t::kK0Short) {
        int phibin = hLocalDensity->FindBin(RecoDecay::constrainAngle(v0Gen.phi(), -constants::math::PI));
        double density = hLocalDensity->Integral(phibin - cfgDeltaPhiLocDen, phibin + cfgDeltaPhiLocDen);
        if (cfgOutputLocDenWeights)
          registry.fill(HIST("MC/densityMCGenK0s"), v0Gen.pt(), nch, density);
        fGFW->Fill(v0Gen.eta(), fXiPtAxis->FindBin(v0Gen.pt()) - 1, v0Gen.phi(), 1, 512);
      }
      if (pdgCode == PDG_t::kLambda0) {
        int phibin = hLocalDensity->FindBin(RecoDecay::constrainAngle(v0Gen.phi(), -constants::math::PI));
        double density = hLocalDensity->Integral(phibin - cfgDeltaPhiLocDen, phibin + cfgDeltaPhiLocDen);
        if (cfgOutputLocDenWeights)
          registry.fill(HIST("MC/densityMCGenLambda"), v0Gen.pt(), nch, density);
        fGFW->Fill(v0Gen.eta(), fXiPtAxis->FindBin(v0Gen.pt()) - 1, v0Gen.phi(), 1, 1024);
      }
    }
    fillProfile(corrconfigs.at(25), HIST("MC/c22MC"), cent);
    for (int i = 1; i <= nV0PtBins; i++) {
      fillProfilepTMC(corrconfigs.at(21), HIST("MC/K0sc22dptMC"), i, kK0Short, cent);
      fillProfilepTMC(corrconfigs.at(22), HIST("MC/K0sc22dptMC"), i, kK0Short, cent);
      fillProfilepTMC(corrconfigs.at(23), HIST("MC/Lambdac22dptMC"), i, kLambda0, cent);
      fillProfilepTMC(corrconfigs.at(24), HIST("MC/Lambdac22dptMC"), i, kLambda0, cent);
    }
    for (int i = 1; i <= nXiPtBins; i++) {
      fillProfilepTMC(corrconfigs.at(17), HIST("MC/Xic22dptMC"), i, kXiMinus, cent);
      fillProfilepTMC(corrconfigs.at(18), HIST("MC/Xic22dptMC"), i, kXiMinus, cent);
      fillProfilepTMC(corrconfigs.at(19), HIST("MC/Omegac22dptMC"), i, kOmegaMinus, cent);
      fillProfilepTMC(corrconfigs.at(20), HIST("MC/Omegac22dptMC"), i, kOmegaMinus, cent);
    }

    delete hLocalDensity;
  }
  PROCESS_SWITCH(FlowGfwOmegaXi, processMC, "", true);
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{
    adaptAnalysisTask<FlowGfwOmegaXi>(cfgc)};
}
