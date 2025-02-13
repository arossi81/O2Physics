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
//
/// \file k0_mixed_events.cxx
/// \brief Femto3D pair mixing task
/// \author Sofia Tomassini, Gleb Romanenko, Nicolò Jacazio
/// \since 31 May 2023

#include <vector>
#include <map>
#include <memory>
#include <utility>

#include <TParameter.h>
#include <TH1F.h>
#include <TLorentzVector.h>

#include "Framework/runDataProcessing.h"
#include "Framework/AnalysisTask.h"
#include "Framework/HistogramRegistry.h"
#include "Framework/ASoA.h"
#include "Framework/DataTypes.h"
#include "Framework/AnalysisDataModel.h"
#include "Framework/Expressions.h"
#include "Framework/StaticFor.h"

#include "MathUtils/Utils.h"
#include "Common/DataModel/Multiplicity.h"

#include "PWGCF/Femto3D/DataModel/singletrackselector.h"
#include "PWGCF/Femto3D/Core/femto3dPairTask.h"

using namespace o2;
using namespace o2::soa;
using namespace o2::aod;
using namespace o2::framework;
using namespace o2::framework::expressions;

using FilteredCollisions = soa::Filtered<aod::SingleCollSels>;
using FilteredTracks = soa::Filtered<soa::Join<aod::SingleTrackSels, aod::SinglePIDEls, aod::SinglePIDPis, aod::SinglePIDKas, aod::SinglePIDPrs, aod::SinglePIDDes, aod::SinglePIDTrs, aod::SinglePIDHes>>;

typedef std::shared_ptr<FilteredTracks::iterator> trkType;
typedef std::shared_ptr<FilteredCollisions::iterator> colType;

using MyFemtoPair = o2::aod::singletrackselector::FemtoPair<trkType>;

class ResoPair : public MyFemtoPair
{
 public:
  ResoPair() {}
  ResoPair(trkType const& first, trkType const& second) : MyFemtoPair(first, second)
  {
    setPair(first, second);
  }
  ResoPair(trkType const& first, trkType const& second, const bool& isidentical) : MyFemtoPair(first, second, isidentical) {}
  bool isClosePair() const { return MyFemtoPair::IsClosePair(mDeltaEta, mDeltaPhi, mRadius); }
  void setEtaDiff(const float deta) { mDeltaEta = deta; }
  void setPhiStarDiff(const float dphi) { mDeltaPhi = dphi; }
  void setPair(trkType const& first, trkType const& second)
  {
    MyFemtoPair::SetPair(first, second);
    lDecayDaughter1.SetPtEtaPhiM(first->pt(), first->eta(), first->phi(), particle_mass(GetPDG1()));
    lDecayDaughter2.SetPtEtaPhiM(second->pt(), second->eta(), second->phi(), particle_mass(GetPDG2()));
    lResonance = lDecayDaughter1 + lDecayDaughter2;
  }
  float getInvMass() const
  {
    // LOG(info) << "Mass = " << lResonance.M() << " 1 " << lDecayDaughter1.M() << " 2 " << lDecayDaughter2.M();
    return lResonance.M();
  }
  float getPt() const { return lResonance.Pt(); }
  float getRapidity() const { return lResonance.Rapidity(); }

 private:
  TLorentzVector lDecayDaughter1, lDecayDaughter2, lResonance;
  float mDeltaEta = 0.01;
  float mDeltaPhi = 0.01;
  float mRadius = 1.2;
};

struct K0MixedEvents {
  HistogramRegistry registry{"registry", {}, OutputObjHandlingPolicy::AnalysisObject};

  Configurable<std::pair<float, float>> multPercentileCut{"multPercentileCut", std::pair<float, float>{-100.f, 1000.f}, "[min., max.] centrality range to keep events within"};
  Configurable<std::pair<float, float>> momentumCut{"momentumCut", std::pair<float, float>{0.f, 100.f}, "[min., max.] momentum range to keep candidates within"};
  Configurable<float> dcaxyCut{"dcaxyCut", -100.f, "dcaXY range to keep candidates within"};
  Configurable<float> dcazCut{"dcazCut", -100.f, "dcaZ range to keep candidates within"};
  Configurable<float> dcaxyExclusionCut{"dcaxyExclusionCut", 100.f, "dcaXY range to discard candidates within"};
  Configurable<float> dcazExclusionCut{"dcazExclusionCut", 100.f, "dcaZ range to discard candidates within"};

  Configurable<float> _eta{"eta", 100.0, "abs eta value limit"};
  Configurable<int16_t> _tpcNClsFound{"minTpcNClsFound", 0, "minimum allowed number of TPC clasters"};
  Configurable<float> _tpcChi2NCl{"tpcChi2NCl", 100.0, "upper limit for chi2 value of a fit over TPC clasters"};
  Configurable<float> _tpcCrossedRowsOverFindableCls{"tpcCrossedRowsOverFindableCls", 0, "lower limit of TPC CrossedRows/FindableCls value"};
  Configurable<int> _tpcNClsShared{"maxTpcNClsShared", 100, "maximum allowed number of TPC shared clasters"};
  Configurable<int> _itsNCls{"minItsNCls", 0, "minimum allowed number of ITS clasters"};
  Configurable<float> _itsChi2NCl{"itsChi2NCl", 100.0, "upper limit for chi2 value of a fit over ITS clasters"};
  Configurable<float> _vertexZ{"VertexZ", 10.0, "abs vertexZ value limit"};
  Configurable<float> _maxy{"_maxy", 100.0, "maximum y of both particles in a pair"};

  Configurable<int> _sign_1{"sign_1", 1, "sign of the first particle in a pair"};
  Configurable<int> _particlePDG_1{"particlePDG_1", 2212, "PDG code of the first particle in a pair to perform PID for (only proton and deurton are supported now)"};
  Configurable<std::vector<float>> _tpcNSigma_1{"tpcNSigma_1", std::vector<float>{-3.0f, 3.0f}, "first particle PID: Nsigma range in TPC before the TOF is used"};
  Configurable<float> _PIDtrshld_1{"PIDtrshld_1", 10.0, "first particle PID: value of momentum from which the PID is done with TOF (before that only TPC is used)"};
  Configurable<std::vector<float>> _tofNSigma_1{"tofNSigma_1", std::vector<float>{-3.0f, 3.0f}, "first particle PID: Nsigma range in TOF"};

  Configurable<int> _sign_2{"sign_2", 1, "sign of the second particle in a pair"};
  Configurable<int> _particlePDG_2{"particlePDG_2", 2212, "PDG code of the second particle in a pair to perform PID for (only proton and deurton are supported now)"};
  Configurable<std::vector<float>> _tpcNSigma_2{"tpcNSigma_2", std::vector<float>{-3.0f, 3.0f}, "second particle PID: Nsigma range in TPC before the TOF is used"};
  Configurable<float> _PIDtrshld_2{"PIDtrshld_2", 10.0, "second particle PID: value of momentum from which the PID is done with TOF (before that only TPC is used)"};
  Configurable<std::vector<float>> _tofNSigma_2{"tofNSigma_2", std::vector<float>{-3.0f, 3.0f}, "second particle PID: Nsigma range in TOF"};

  Configurable<int> _particlePDGtoReject{"particlePDGtoRejectFromSecond", 0, "applied only if the particles are non-identical and only to the second particle in the pair!!!"};
  Configurable<std::vector<float>> _rejectWithinNsigmaTOF{"rejectWithinNsigmaTOF", std::vector<float>{-0.0f, 0.0f}, "TOF rejection Nsigma range for the particle specified with PDG to be rejected"};

  Configurable<float> _deta{"deta", 0.01, "minimum allowed defference in eta between two tracks in a pair"};
  Configurable<float> _dphi{"dphi", 0.01, "minimum allowed defference in phi_star between two tracks in a pair"};
  Configurable<float> _radiusTPC{"radiusTPC", 1.2, "TPC radius to calculate phi_star for"};

  Configurable<bool> doMixedEvent{"doMixedEvent", false, "Do the mixed event"};
  Configurable<int> _multbinwidth{"multbinwidth", 50, "width of multiplicity bins within which the mixing is done"};
  Configurable<int> _vertexbinwidth{"vertexbinwidth", 2, "width of vertexZ bins within which the mixing is done"};

  // Binnings
  ConfigurableAxis CFkStarBinning{"CFkStarBinning", {500, 0.4, 0.6}, "k* binning of the CF (Nbins, lowlimit, uplimit)"};
  ConfigurableAxis ptBinning{"ptBinning", {1000, 0.f, 10.f}, "pT binning (Nbins, lowlimit, uplimit)"};
  ConfigurableAxis dcaXyBinning{"dcaXyBinning", {100, -1.f, 1.f}, "dcaXY binning (Nbins, lowlimit, uplimit)"};

  bool IsIdentical;

  std::pair<int, std::vector<float>> TPCcuts_1;
  std::pair<int, std::vector<float>> TOFcuts_1;

  std::pair<int, std::vector<float>> TPCcuts_2;
  std::pair<int, std::vector<float>> TOFcuts_2;

  std::map<int64_t, std::vector<trkType>> selectedtracks_1;
  std::map<int64_t, std::vector<trkType>> selectedtracks_2;
  std::map<std::pair<int, int>, std::vector<colType>> mixbins;

  std::unique_ptr<ResoPair> Pair = std::make_unique<ResoPair>();

  Filter pFilter = o2::aod::singletrackselector::p > momentumCut.value.first&& o2::aod::singletrackselector::p < momentumCut.value.second;
  Filter etaFilter = nabs(o2::aod::singletrackselector::eta) < _eta;
  Filter tpcTrkFilter = o2::aod::singletrackselector::tpcNClsFound >= _tpcNClsFound && o2::aod::singletrackselector::tpcNClsShared <= (uint8_t)_tpcNClsShared;

  // Filter itsNClsFilter = o2::aod::singletrackselector::itsNCls >= (uint8_t)_itsNCls;

  Filter vertexFilter = nabs(o2::aod::singletrackselector::posZ) < _vertexZ;
  Filter multPercentileFilter = o2::aod::singletrackselector::multPerc > multPercentileCut.value.first&& o2::aod::singletrackselector::multPerc < multPercentileCut.value.second;

  const char* pdgToSymbol(const int pdg)
  {
    switch (std::abs(pdg)) {
      case 211:
        return "#pi";
      case 321:
        return "K";
      case 2212:
        return "p";
      case 1000010020:
        return "d";
    }
    return "X";
  }

  void init(o2::framework::InitContext&)
  {
    IsIdentical = (_sign_1 * _particlePDG_1 == _sign_2 * _particlePDG_2);
    LOG(info) << "IsIdentical=" << IsIdentical << "; sign1=" << _sign_1 << "; Pdg1=" << _particlePDG_1 << "; total1=" << _sign_1 * _particlePDG_1 << " -- Pdg2=" << _particlePDG_2 << "; sign2=" << _sign_2 << "; total2=" << _sign_2 * _particlePDG_2;

    Pair->SetIdentical(IsIdentical);
    Pair->SetPDG1(_particlePDG_1);
    Pair->SetPDG2(_particlePDG_2);
    Pair->setEtaDiff(1);

    TPCcuts_1 = std::make_pair(_particlePDG_1, _tpcNSigma_1);
    TOFcuts_1 = std::make_pair(_particlePDG_1, _tofNSigma_1);
    TPCcuts_2 = std::make_pair(_particlePDG_2, _tpcNSigma_2);
    TOFcuts_2 = std::make_pair(_particlePDG_2, _tofNSigma_2);

    const AxisSpec invMassAxis{CFkStarBinning, "Inv. mass (GeV/c^{2})"};
    const AxisSpec ptAxis{ptBinning, "#it{p}_{T} (GeV/c)"};
    const AxisSpec dcaXyAxis{dcaXyBinning, "DCA_{xy} (cm)"};

    registry.add("Trks", "Trks", kTH1D, {{2, 0.5, 2.5, "Tracks"}});
    registry.add("VTXc", "VTXc", kTH1F, {{100, -20., 20., "vtx"}});
    registry.add("VTX", "VTX", kTH1F, {{100, -20., 20., "vtx"}});
    registry.add("SEcand", "SEcand", kTH1F, {{2, 0.5, 2.5}});
    registry.add("SE", "SE", kTH1F, {invMassAxis});
    registry.add("ME", "ME", kTH1F, {invMassAxis});
    registry.add("SEvsPt", "SEvsPt", kTH2D, {invMassAxis, ptAxis});
    registry.add("MEvsPt", "MEvsPt", kTH2D, {invMassAxis, ptAxis});
    registry.add("eta", Form("eta_%i", _particlePDG_1.value), kTH2F, {ptAxis, {100, -10., 10., "#eta"}});
    registry.add("p_first", Form("p_%i", _particlePDG_1.value), kTH1F, {ptAxis});
    registry.add("dcaXY_first", Form("dca_%i", _particlePDG_1.value), kTH2F, {ptAxis, dcaXyAxis});
    registry.add("nsigmaTOF_first", Form("nsigmaTOF_%i", _particlePDG_1.value), kTH2F, {ptAxis, {100, -10., 10., Form("N#sigma_{TOF}(%s))", pdgToSymbol(_particlePDG_1))}});
    registry.add("nsigmaTPC_first", Form("nsigmaTPC_%i", _particlePDG_1.value), kTH2F, {ptAxis, {100, -10., 10., Form("N#sigma_{TPC}(%s))", pdgToSymbol(_particlePDG_1))}});
    registry.add("rapidity_first", Form("rapidity_%i", _particlePDG_1.value), kTH2F, {ptAxis, {100, -10., 10., Form("y(%s)", pdgToSymbol(_particlePDG_1))}});

    if (!IsIdentical) {
      registry.add("p_second", Form("p_%i", _particlePDG_2.value), kTH1F, {ptAxis});
      registry.add("dcaXY_second", Form("dca_%i", _particlePDG_2.value), kTH2F, {ptAxis, dcaXyAxis});
      registry.add("nsigmaTOF_second", Form("nsigmaTOF_%i", _particlePDG_2.value), kTH2F, {ptAxis, {100, -10., 10., Form("N#sigma_{TOF}(%s))", pdgToSymbol(_particlePDG_2))}});
      registry.add("nsigmaTPC_second", Form("nsigmaTPC_%i", _particlePDG_2.value), kTH2F, {ptAxis, {100, -10., 10., Form("N#sigma_{TPC}(%s))", pdgToSymbol(_particlePDG_2))}});
      registry.add("rapidity_second", Form("rapidity_%i", _particlePDG_2.value), kTH2F, {ptAxis, {100, -10., 10., Form("y(%s)", pdgToSymbol(_particlePDG_2))}});
    }
  }

  template <typename Type>
  void mixTracks(Type const& tracks)
  { // template for identical particles from the same collision

    LOG(debug) << "Mixing tracks of the same event";
    for (uint32_t trk1 = 0; trk1 < tracks.size(); trk1++) { // nested loop for all the combinations
      for (uint32_t trk2 = trk1 + 1; trk2 < tracks.size(); trk2++) {

        Pair->setPair(tracks[trk1], tracks[trk2]);

        registry.fill(HIST("SEcand"), 1.f);
        if (!Pair->isClosePair()) {
          continue;
        }
        if (std::abs(Pair->getRapidity()) > 0.5f) {
          continue;
        }
        registry.fill(HIST("SEcand"), 2.f);
        registry.fill(HIST("SE"), Pair->getInvMass());                    // close pair rejection and fillig the SE histo
        registry.fill(HIST("SEvsPt"), Pair->getInvMass(), Pair->getPt()); // close pair rejection and fillig the SE histo
      }
    }
  }

  template <bool isSameEvent = false, typename Type>
  void mixTracks(Type const& tracks1, Type const& tracks2)
  {
    LOG(debug) << "Mixing tracks of two different events";
    for (auto trk1 : tracks1) {
      for (auto trk2 : tracks2) {

        Pair->setPair(trk1, trk2);

        if constexpr (isSameEvent) {
          registry.fill(HIST("SEcand"), 1.f);
        }
        if (!Pair->isClosePair()) {
          continue;
        }
        if (std::abs(Pair->getRapidity()) > 0.5f) {
          continue;
        }
        if constexpr (isSameEvent) {
          registry.fill(HIST("SEcand"), 2.f);
          registry.fill(HIST("SE"), Pair->getInvMass());
          registry.fill(HIST("SEvsPt"), Pair->getInvMass(), Pair->getPt());
        } else {
          registry.fill(HIST("ME"), Pair->getInvMass());
          registry.fill(HIST("MEvsPt"), Pair->getInvMass(), Pair->getPt());
        }
      }
    }
  }

  void process(FilteredTracks const& tracks, FilteredCollisions const& collisions)
  {
    LOG(debug) << "Processing " << collisions.size() << " collisions and " << tracks.size() << " tracks";

    if (_particlePDG_1 == 0 || _particlePDG_2 == 0) {
      LOGF(fatal, "One of passed PDG is 0!!!");
    }
    registry.fill(HIST("Trks"), 2.f, tracks.size());
    for (auto collision : collisions) {
      LOG(debug) << "Collision index " << collision.globalIndex();
      registry.fill(HIST("VTXc"), collision.posZ());
    }

    for (auto track : tracks) {
      LOG(debug) << "Track index " << track.singleCollSelId();
      if (track.itsNCls() < _itsNCls) {
        continue;
      }
      if (track.itsChi2NCl() > _itsChi2NCl) {
        continue;
      }
      if (track.tpcChi2NCl() > _tpcChi2NCl) {
        continue;
      }
      if (track.tpcCrossedRowsOverFindableCls() < _tpcCrossedRowsOverFindableCls) {
        continue;
      }
      if (std::abs(track.dcaXY()) > dcaxyCut) {
        continue;
      }
      if (std::abs(track.dcaXY()) < dcaxyExclusionCut) {
        continue;
      }
      if (std::abs(track.dcaZ()) > dcazCut) {
        continue;
      }
      if (std::abs(track.dcaZ()) < dcazExclusionCut) {
        continue;
      }

      registry.fill(HIST("Trks"), 1);
      const auto& col = track.singleCollSel_as<FilteredCollisions>();
      registry.fill(HIST("VTX"), col.posZ());
      if (std::abs(col.posZ()) > _vertexZ)
        continue;
      if (col.multPerc() > multPercentileCut.value.second || col.multPerc() < multPercentileCut.value.first)
        continue;
      registry.fill(HIST("eta"), track.pt(), track.eta());
      if (std::abs(track.rapidity(particle_mass(_particlePDG_1))) > _maxy) {
        continue;
      }
      registry.fill(HIST("rapidity_first"), track.pt(), track.rapidity(particle_mass(_particlePDG_1)));

      if ((track.sign() == _sign_1) &&
          (track.p() < _PIDtrshld_1 ? o2::aod::singletrackselector::TPCselection(track, TPCcuts_1) : o2::aod::singletrackselector::TOFselection(track, TOFcuts_1))) { // filling the map: eventID <-> selected particles1
        selectedtracks_1[track.singleCollSelId()].push_back(std::make_shared<decltype(track)>(track));

        registry.fill(HIST("p_first"), track.p());
        registry.fill(HIST("dcaXY_first"), track.pt(), track.dcaXY());
        switch (_particlePDG_1) {
          case 211:
            registry.fill(HIST("nsigmaTOF_first"), track.p(), track.tofNSigmaPi());
            registry.fill(HIST("nsigmaTPC_first"), track.p(), track.tpcNSigmaPi());
            break;
          case 321:
            registry.fill(HIST("nsigmaTOF_first"), track.p(), track.tofNSigmaKa());
            registry.fill(HIST("nsigmaTPC_first"), track.p(), track.tpcNSigmaKa());
            break;
          case 2212:
            registry.fill(HIST("nsigmaTOF_first"), track.p(), track.tofNSigmaPr());
            registry.fill(HIST("nsigmaTPC_first"), track.p(), track.tpcNSigmaPr());
            break;
          case 1000010020:
            registry.fill(HIST("nsigmaTOF_first"), track.p(), track.tofNSigmaDe());
            registry.fill(HIST("nsigmaTPC_first"), track.p(), track.tpcNSigmaDe());
            break;
          default:
            LOG(fatal) << "PDG code 1: " << _particlePDG_1 << " is not supported!!!";
        }
      }

      if (IsIdentical) {
        continue;
      } else if ((track.sign() == _sign_2) &&
                 (_particlePDGtoReject != 0 || !TOFselection(track, std::make_pair(_particlePDGtoReject, _rejectWithinNsigmaTOF))) &&
                 (track.p() < _PIDtrshld_2 ? o2::aod::singletrackselector::TPCselection(track, TPCcuts_2) : o2::aod::singletrackselector::TOFselection(track, TOFcuts_2))) { // filling the map: eventID <-> selected particles2 if (see condition above ^)
        selectedtracks_2[track.singleCollSelId()].push_back(std::make_shared<decltype(track)>(track));

        registry.fill(HIST("p_second"), track.p());
        registry.fill(HIST("dcaXY_second"), track.pt(), track.dcaXY());
        switch (_particlePDG_2) {
          case 211:
            registry.fill(HIST("nsigmaTOF_second"), track.p(), track.tofNSigmaPi());
            registry.fill(HIST("nsigmaTPC_second"), track.p(), track.tpcNSigmaPi());
            break;
          case 321:
            registry.fill(HIST("nsigmaTOF_second"), track.p(), track.tofNSigmaKa());
            registry.fill(HIST("nsigmaTPC_second"), track.p(), track.tpcNSigmaKa());
            break;
          case 2212:
            registry.fill(HIST("nsigmaTOF_second"), track.p(), track.tofNSigmaPr());
            registry.fill(HIST("nsigmaTPC_second"), track.p(), track.tpcNSigmaPr());
            break;
          case 1000010020:
            registry.fill(HIST("nsigmaTOF_second"), track.p(), track.tofNSigmaDe());
            registry.fill(HIST("nsigmaTPC_second"), track.p(), track.tpcNSigmaDe());
            break;
          default:
            LOG(fatal) << "PDG code 2: " << _particlePDG_2 << " is not supported!!!";
        }
      }
    }

    for (auto collision : collisions) {
      if (selectedtracks_1.find(collision.globalIndex()) == selectedtracks_1.end()) {
        if (IsIdentical)
          continue;
        else if (selectedtracks_2.find(collision.globalIndex()) == selectedtracks_2.end())
          continue;
      }

      mixbins[std::pair<int, int>{round(collision.posZ() / _vertexbinwidth), floor(collision.mult() / _multbinwidth)}].push_back(std::make_shared<decltype(collision)>(collision));
    }

    //====================================== mixing starts here ======================================

    if (IsIdentical) { //====================================== mixing identical ======================================

      for (auto i = mixbins.begin(); i != mixbins.end(); i++) { // iterating over all vertex&mult bins

        for (uint32_t indx1 = 0; indx1 < (i->second).size(); indx1++) { // loop over all the events in each vertex&mult bin

          auto col1 = (i->second)[indx1];

          Pair->SetMagField1(col1->magField());
          Pair->SetMagField2(col1->magField());

          mixTracks(selectedtracks_1[col1->index()]); // mixing SE identical
          if (!doMixedEvent) {
            continue;
          }

          for (uint32_t indx2 = indx1 + 1; indx2 < (i->second).size(); indx2++) { // nested loop for all the combinations of collisions in a chosen mult/vertex bin

            auto col2 = (i->second)[indx2];

            Pair->SetMagField2(col2->magField());
            mixTracks(selectedtracks_1[col1->index()], selectedtracks_1[col2->index()]); // mixing ME identical
          }
        }
      }

      //====================================== end of mixing identical ======================================
    } else {
      //====================================== mixing non-identical ======================================

      for (auto i = mixbins.begin(); i != mixbins.end(); i++) { // iterating over all vertex&mult bins

        for (uint32_t indx1 = 0; indx1 < (i->second).size(); indx1++) { // loop over all the events in each vertex&mult bin

          auto col1 = (i->second)[indx1];

          Pair->SetMagField1(col1->magField());
          Pair->SetMagField2(col1->magField());

          mixTracks<true>(selectedtracks_1[col1->index()], selectedtracks_2[col1->index()]); // mixing SE non-identical
          if (!doMixedEvent) {
            continue;
          }

          for (uint32_t indx2 = indx1 + 1; indx2 < (i->second).size(); indx2++) { // nested loop for all the combinations of collisions in a chosen mult/vertex bin

            auto col2 = (i->second)[indx2];

            Pair->SetMagField2(col2->magField());
            mixTracks(selectedtracks_1[col1->index()], selectedtracks_2[col2->index()]); // mixing ME non-identical
          }
        }
      }

    } //====================================== end of mixing non-identical ======================================

    // clearing up
    for (auto i = selectedtracks_1.begin(); i != selectedtracks_1.end(); i++)
      (i->second).clear();
    selectedtracks_1.clear();

    if (!IsIdentical) {
      for (auto i = selectedtracks_2.begin(); i != selectedtracks_2.end(); i++)
        (i->second).clear();
      selectedtracks_2.clear();
    }

    for (auto i = mixbins.begin(); i != mixbins.end(); i++)
      (i->second).clear();
    mixbins.clear();
  }
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{adaptAnalysisTask<K0MixedEvents>(cfgc)};
}
