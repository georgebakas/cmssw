//
// Original Author:  Elisabetta Manca
//         Created:  Wed, 01 Aug 2018 12:02:51 GMT
//
//


// system include files
#include <memory>

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDFilter.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/StreamID.h"

#include "DataFormats/TrackReco/interface/TrackFwd.h"
#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/TrackingRecHit/interface/TrackingRecHit.h"
#include "DataFormats/TrackingRecHit/interface/TrackingRecHitFwd.h"
#include "DataFormats/SiStripDetId/interface/SiStripDetId.h"
#include "TrackingTools/PatternTools/interface/Trajectory.h"
#include "TrackingTools/TrackFitters/interface/TrajectoryStateCombiner.h"

#include "Geometry/CommonDetUnit/interface/GlobalTrackingGeometry.h"
#include "Geometry/Records/interface/GlobalTrackingGeometryRecord.h"
#include "DataFormats/GeometryCommonDetAlgo/interface/MeasurementPoint.h"
#include "Geometry/CommonTopologies/interface/StripTopology.h"
#include "Geometry/CommonTopologies/interface/TkRadialStripTopology.h"

#include "SimDataFormats/TrackingHit/interface/PSimHit.h"
#include "SimDataFormats/TrackingHit/interface/PSimHitContainer.h"
#include "SimTracker/TrackerHitAssociation/interface/TrackerHitAssociator.h"

#include "RecoTracker/TransientTrackingRecHit/interface/TkTransientTrackingRecHitBuilder.h"
#include "TrackingTools/Records/interface/TransientRecHitRecord.h"
#include "RecoLocalTracker/ClusterParameterEstimator/interface/FakeCPE.h"
#include "RecoLocalTracker/ClusterParameterEstimator/interface/PixelFakeCPE.h"
#include "RecoLocalTracker/ClusterParameterEstimator/interface/StripFakeCPE.h"


//
// class declaration
//

class FakeCPEFiller final : public edm::one::EDFilter<> {
   public:
      explicit FakeCPEFiller(const edm::ParameterSet&);
      ~FakeCPEFiller() = default;

      static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

   private:
      bool filter(edm::Event&, const edm::EventSetup&) override;

      // ----------member data ---------------------------
  
  edm::EDGetTokenT<std::vector<Trajectory> >      inputTraj_;
  edm::EDGetTokenT< edm::DetSetVector<StripDigiSimLink> > stripdigi_token_;
  edm::EDGetTokenT< edm::DetSetVector<PixelDigiSimLink> > pxldigi_token_;
  TrackerHitAssociator::Config trackerHitAssociatorConfig_;

  FakeCPE fakeCPE;


};

//
// constants, enums and typedefs
//

//
// static data member definitions
//

//
// constructors and destructor
//
FakeCPEFiller::FakeCPEFiller(const edm::ParameterSet& iConfig):  trackerHitAssociatorConfig_(iConfig,consumesCollector())

{
   //now do what ever initialization is needed
  inputTraj_ = consumes<std::vector<Trajectory> >(edm::InputTag("FinalTracks"));
  stripdigi_token_=consumes< edm::DetSetVector<StripDigiSimLink> >(iConfig.getParameter<edm::InputTag>("stripSimLinkSrc"));
  pxldigi_token_=consumes< edm::DetSetVector<PixelDigiSimLink> >(iConfig.getParameter<edm::InputTag>("pixelSimLinkSrc"));

}


// ------------ method called on each new Event  ------------
bool
FakeCPEFiller::filter(edm::Event& iEvent, const edm::EventSetup& iSetup)
{

   fakeCPE.map().clear();

   edm::ESHandle<TransientTrackingRecHitBuilder> theB;
   iSetup.get<TransientRecHitRecord>().get("Fake",theB);

   auto const & ttb = static_cast<TkTransientTrackingRecHitBuilder const &>(*theB);
   const_cast<StripFakeCPE*>(static_cast<StripFakeCPE const *>(ttb.stripClusterParameterEstimator()))->setFakeCPE(&fakeCPE);
   const_cast<PixelFakeCPE*>(static_cast<PixelFakeCPE const *>(ttb.pixelClusterParameterEstimator()))->setFakeCPE(&fakeCPE);
  


   using namespace edm;

   edm::ESHandle<GlobalTrackingGeometry> globalGeometry;
   iSetup.get<GlobalTrackingGeometryRecord>().get(globalGeometry);
   
   Handle<std::vector<Trajectory> > trajH;
   iEvent.getByToken(inputTraj_,trajH);
   
   TrackerHitAssociator HitAssoc(iEvent, trackerHitAssociatorConfig_);
   
   for (unsigned int j =0 ; j<trajH->size();++j) {

     const std::vector<TrajectoryMeasurement> &tms = (*trajH)[j].measurements();

     for (unsigned int i=0;i<tms.size();++i) {      
       TrajectoryStateOnSurface updatedState   = tms[i].updatedState();

       if (!updatedState.isValid()) continue;
       
       if (!tms[i].recHit()->isValid()) continue;
   
       auto const & thit = static_cast<BaseTrackerRecHit const&>(*tms[i].recHit());
       auto const & clus = thit.firstClusterRef();
//       if (clus.isPixel())
          
       auto const & simHits = HitAssoc.associateHit(*(tms[i].recHit()));

       for (auto const & sh : simHits) {
	 
	 if(sh.processType() !=0) continue;

	 std::cout << sh.localPosition().x() << std::endl;
	 std::cout << sh.localPosition().y() << std::endl;
	 std::cout << sh.localPosition().z() << std::endl;

         using LocalValues = std::pair<LocalPoint,LocalError>;
         //void add(const Cluster& cluster, const GeomDetUnit& gd,LocalValues const & lv) { m_map[encode(cluster,gd)] = lv; }

         LocalValues lv(sh.localPosition(),thit.localPositionError());
	 // Fill The Map
         if (clus.isPixel())
              fakeCPE.map().add(clus.pixelCluster(), *thit.detUnit(),lv);
         else
             fakeCPE.map().add(clus.stripCluster(), *thit.detUnit(),lv);
 
         break; 

       } // closes loop on simHits
       
    
     } // closes loop on trajectory measurements
     
   } // closes loop on trajectories

  return true; // can be false if no good traj found?   
}
 
// ------------ method fills 'descriptions' with the allowed parameters for the module  ------------
void
FakeCPEFiller::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  //The following says we do not know what parameters are allowed so do no validation
  // Please change this to state exactly what you do use, even if it is no parameters
  edm::ParameterSetDescription desc;
  desc.setUnknown();
  descriptions.addDefault(desc);
}
//define this as a plug-in
DEFINE_FWK_MODULE(FakeCPEFiller);

