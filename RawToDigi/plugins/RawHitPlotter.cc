#include <iostream>
#include "TH1F.h"
#include "TH2F.h"
#include "TH2Poly.h"
#include <fstream>
#include <sstream>
// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"

#include "CommonTools/UtilAlgos/interface/TFileService.h"

#include "HGCal/DataFormats/interface/HGCalTBRawHitCollection.h"
#include "HGCal/DataFormats/interface/HGCalTBDetId.h"
#include "HGCal/CondObjects/interface/HGCalElectronicsMap.h"
#include "HGCal/CondObjects/interface/HGCalCondObjectTextIO.h"
#include "HGCal/DataFormats/interface/HGCalTBElectronicsId.h"
#include "HGCal/Geometry/interface/HGCalTBCellVertices.h"
#include "HGCal/Geometry/interface/HGCalTBTopology.h"
#include <iomanip>
#include <set>

const static size_t N_HEXABOARDS = 1;
const static size_t N_SKIROC_PER_HEXA = 4;
const static size_t N_CHANNELS_PER_SKIROC = 64;

#define MAXVERTICES 6
static const double delta = 0.00001;//Add/subtract delta = 0.00001 to x,y of a cell centre so the TH2Poly::Fill doesnt have a problem at the edges where the centre of a half-hex cell passes through the sennsor boundary line.

class RawHitPlotter : public edm::one::EDAnalyzer<edm::one::SharedResources>
{
public:
  explicit RawHitPlotter(const edm::ParameterSet&);
  ~RawHitPlotter();
  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);
private:
  virtual void beginJob() override;
  void analyze(const edm::Event& , const edm::EventSetup&) override;
  virtual void endJob() override;
  void InitTH2Poly(TH2Poly& poly, int layerID, int sensorIU, int sensorIV);

  std::string m_electronicMap;

  struct {
    HGCalElectronicsMap emap_;
  } essource_;
  int m_sensorsize;
  bool m_eventPlotter;
  bool m_subtractCommonMode;
  double m_commonModeThreshold; //number of sigmas from ped mean

  int m_evtID;
  std::map<int,TH1F*> m_h_adcHigh;
  std::map<int,TH1F*> m_h_adcLow;
  std::map<int,TH2F*> m_h_pulseHigh;
  std::map<int,TH2F*> m_h_pulseLow;

  edm::EDGetTokenT<HGCalTBRawHitCollection> m_HGCalTBRawHitCollection;

  HGCalTBTopology IsCellValid;
  HGCalTBCellVertices TheCell;
  std::vector<std::pair<double, double>> CellXY;
  std::pair<double, double> CellCentreXY;
  std::set< std::pair<int,HGCalTBDetId> > setOfConnectedDetId;

  struct commonModeNoise{
    commonModeNoise():fullHG(0),halfHG(0),mouseBiteHG(0),outerHG(0),fullLG(0),halfLG(0),mouseBiteLG(0),outerLG(0),fullCounter(0),halfCounter(0),mouseBiteCounter(0),outerCounter(0){;}
    float fullHG,halfHG,mouseBiteHG,outerHG;
    float fullLG,halfLG,mouseBiteLG,outerLG;
    int fullCounter,halfCounter,mouseBiteCounter,outerCounter;
  };

};

RawHitPlotter::RawHitPlotter(const edm::ParameterSet& iConfig) :
  m_electronicMap(iConfig.getUntrackedParameter<std::string>("ElectronicMap","HGCal/CondObjects/data/map_CERN_Hexaboard_OneLayers_May2017.txt")),
  m_sensorsize(iConfig.getUntrackedParameter<int>("SensorSize",128)),
  m_eventPlotter(iConfig.getUntrackedParameter<bool>("EventPlotter",false)),
  m_subtractCommonMode(iConfig.getUntrackedParameter<bool>("SubtractCommonMode",false)),
  m_commonModeThreshold(iConfig.getUntrackedParameter<double>("CommonModeThreshold",100))
{
  usesResource("TFileService");
  edm::Service<TFileService> fs;

  m_HGCalTBRawHitCollection = consumes<HGCalTBRawHitCollection>(iConfig.getParameter<edm::InputTag>("InputCollection"));

  m_evtID=0;
  
  std::ostringstream os( std::ostringstream::ate );
  TH2F* htmp2;
  TH1F* htmp1;
  for(size_t ib = 0; ib<N_HEXABOARDS; ib++) {
    for( size_t iski=0; iski<N_SKIROC_PER_HEXA; iski++ ){
      os.str("");os<<"HexaBoard"<<ib<<"_Skiroc"<<iski;
      TFileDirectory dir = fs->mkdir( os.str().c_str() );
      for( size_t ichan=0; ichan<N_CHANNELS_PER_SKIROC; ichan++ ){
	for( size_t it=0; it<NUMBER_OF_TIME_SAMPLES-0; it++ ){
	  os.str("");
	  os << "HighGain_HexaBoard" << ib << "_Chip" << iski << "_Channel" << ichan << "_Sample" << it ;
	  htmp1=dir.make<TH1F>(os.str().c_str(),os.str().c_str(),4000,-500,3500);
	  m_h_adcHigh.insert( std::pair<int,TH1F*>(ib*100000+iski*10000+ichan*100+it, htmp1) );
	  os.str("");
	  os << "LowGain_HexaBoard" << ib << "_Chip" << iski << "_Channel" << ichan << "_Sample" << it ;
	  htmp1=dir.make<TH1F>(os.str().c_str(),os.str().c_str(),4000,-500,3500);
	  m_h_adcLow.insert( std::pair<int,TH1F*>(ib*100000+iski*10000+ichan*100+it, htmp1) );
	}
	os.str("");
	os << "PulseHighGain_Hexa" << ib << "_Chip" << iski << "_Channel" << ichan;
	htmp2=dir.make<TH2F>(os.str().c_str(),os.str().c_str(),NUMBER_OF_TIME_SAMPLES-0,0,(NUMBER_OF_TIME_SAMPLES-0)*25,4000,-500,3500);
	m_h_pulseHigh.insert( std::pair<int,TH2F*>(ib*1000+iski*100+ichan, htmp2) );
	os.str("");
	os << "PulseLowGain_Hexa" << ib << "_Chip" << iski << "_Channel" << ichan;
	htmp2=dir.make<TH2F>(os.str().c_str(),os.str().c_str(),NUMBER_OF_TIME_SAMPLES-0,0,(NUMBER_OF_TIME_SAMPLES-0)*25,4000,-500,3500);
	m_h_pulseLow.insert( std::pair<int,TH2F*>(ib*1000+iski*100+ichan, htmp2) );
      }
    }
  }
  std::cout << iConfig.dump() << std::endl;
}


RawHitPlotter::~RawHitPlotter()
{

}

void RawHitPlotter::beginJob()
{
  HGCalCondObjectTextIO io(0);
  edm::FileInPath fip(m_electronicMap);
  if (!io.load(fip.fullPath(), essource_.emap_)) {
    throw cms::Exception("Unable to load electronics map");
  };  
}

void RawHitPlotter::analyze(const edm::Event& event, const edm::EventSetup& setup)
{
  usesResource("TFileService");
  edm::Service<TFileService> fs;

  edm::Handle<HGCalTBRawHitCollection> hits;
  event.getByToken(m_HGCalTBRawHitCollection, hits);
  
  std::map<int,TH2Poly*>  polyMap;
  if( m_eventPlotter ){
    std::ostringstream os( std::ostringstream::ate );
    os << "Event" << event.id().event();
    TFileDirectory dir = fs->mkdir( os.str().c_str() );
    for(size_t ib = 0; ib<N_HEXABOARDS; ib++) {
      for( size_t it=0; it<NUMBER_OF_TIME_SAMPLES-0; it++ ){
	TH2Poly *h=dir.make<TH2Poly>();
	os.str("");
	os<<"HexaBoard"<<ib<<"_TimeSample"<<it;
	h->SetName(os.str().c_str());
	h->SetTitle(os.str().c_str());
	InitTH2Poly(*h, (int)ib, 0, 0);
	polyMap.insert( std::pair<int,TH2Poly*>(100*ib+it,h) );
      }
    }
  }
  
  commonModeNoise cm[NUMBER_OF_TIME_SAMPLES-0][4];
  if( m_subtractCommonMode ){
    for( auto hit : *hits ){
      //      int iboard=hit.skiroc()/N_SKIROC_PER_HEXA;
      int iski=hit.skiroc();
      //int ichan=hit.channel();
      if( !essource_.emap_.existsDetId(hit.detid()) ) continue;
      for( size_t it=0; it<NUMBER_OF_TIME_SAMPLES-0; it++ ){
	if( hit.highGainADC(it)>m_commonModeThreshold ) continue;
	float highGain = hit.highGainADC(it);
	float lowGain = hit.lowGainADC(it);
	switch ( hit.detid().cellType() ){
	case 0 : cm[it][iski].fullHG += highGain; cm[it][iski].fullLG += lowGain; cm[it][iski].fullCounter++; break;
	case 2 : cm[it][iski].halfHG += highGain; cm[it][iski].halfLG += lowGain; cm[it][iski].halfCounter++; break;
	case 3 : cm[it][iski].mouseBiteHG += highGain; cm[it][iski].mouseBiteLG += lowGain; cm[it][iski].mouseBiteCounter++; break;
	case 4 : cm[it][iski].outerHG += highGain; cm[it][iski].outerLG += lowGain; cm[it][iski].outerCounter++; break;
	// case 0 : cm[it].fullHG += highGain; cm[it].fullLG += lowGain; cm[it].fullCounter++; break;
	// case 2 : cm[it].halfHG += highGain; cm[it].halfLG += lowGain; cm[it].halfCounter++; break;
	// case 3 : cm[it].mouseBiteHG += highGain; cm[it].mouseBiteLG += lowGain; cm[it].mouseBiteCounter++; break;
	// case 4 : cm[it].outerHG += highGain; cm[it].outerLG += lowGain; cm[it].outerCounter++; break;
	}
      }
    }
  }
  for( auto hit : *hits ){
    int iboard=hit.skiroc()/N_SKIROC_PER_HEXA;
    int iski=hit.skiroc();
    int ichan=hit.channel();
    for( size_t it=0; it<NUMBER_OF_TIME_SAMPLES-0; it++ ){
      float highGain,lowGain;
      if( m_subtractCommonMode && essource_.emap_.existsDetId(hit.detid()) ){
	float subHG(0),subLG(0);
	switch ( hit.detid().cellType() ){
	case 0 : subHG=cm[it][iski].fullHG/cm[it][iski].fullCounter; subLG=cm[it][iski].fullLG/cm[it][iski].fullCounter; break;
	case 2 : subHG=cm[it][iski].halfHG/cm[it][iski].halfCounter; subLG=cm[it][iski].halfLG/cm[it][iski].halfCounter; break;
	case 3 : subHG=cm[it][iski].mouseBiteHG/cm[it][iski].mouseBiteCounter; subLG=cm[it][iski].mouseBiteLG/cm[it][iski].mouseBiteCounter; break;
	case 4 : subHG=cm[it][iski].outerHG/cm[it][iski].outerCounter; subLG=cm[it][iski].outerLG/cm[it][iski].outerCounter; break;
	// case 0 : subHG=cm[it].fullHG/cm[it].fullCounter; subLG=cm[it].fullLG/cm[it].fullCounter; break;
	// case 2 : subHG=cm[it].halfHG/cm[it].halfCounter; subLG=cm[it].halfLG/cm[it].halfCounter; break;
	// case 3 : subHG=cm[it].mouseBiteHG/cm[it].mouseBiteCounter; subLG=cm[it].mouseBiteLG/cm[it].mouseBiteCounter; break;
	// case 4 : subHG=cm[it].outerHG/cm[it].outerCounter; subLG=cm[it].outerLG/cm[it].outerCounter; break;
	}
	highGain=hit.highGainADC(it)-subHG;
	lowGain=hit.lowGainADC(it)-subLG;
      }
      else{
	highGain=hit.highGainADC(it);
	lowGain=hit.lowGainADC(it);
      }
      m_h_adcHigh[iboard*100000+iski*10000+ichan*100+it]->Fill(highGain);
      m_h_adcLow[iboard*100000+iski*10000+ichan*100+it]->Fill(lowGain);
      m_h_pulseHigh[iboard*1000+iski*100+ichan]->Fill(it*25,highGain);
      m_h_pulseLow[iboard*1000+iski*100+ichan]->Fill(it*25,lowGain);
      if( !essource_.emap_.existsDetId(hit.detid()) ) continue;
      std::pair<int,HGCalTBDetId> p( iboard*1000+iski*100+ichan,hit.detid() );
      setOfConnectedDetId.insert(p);
      if(!IsCellValid.iu_iv_valid(hit.detid().layer(),hit.detid().sensorIU(),hit.detid().sensorIV(),hit.detid().iu(),hit.detid().iv(),m_sensorsize))  continue;
      if(m_eventPlotter){
      	CellCentreXY=TheCell.GetCellCentreCoordinatesForPlots(hit.detid().layer(),hit.detid().sensorIU(),hit.detid().sensorIV(),hit.detid().iu(),hit.detid().iv(),m_sensorsize);
      	double iux = (CellCentreXY.first < 0 ) ? (CellCentreXY.first + delta) : (CellCentreXY.first - delta) ;
      	double iuy = (CellCentreXY.second < 0 ) ? (CellCentreXY.second + delta) : (CellCentreXY.second - delta);
      	polyMap[ 100*iboard+it ]->Fill(iux/2 , iuy, highGain);
      }
    }
  }
}

void RawHitPlotter::InitTH2Poly(TH2Poly& poly, int layerID, int sensorIU, int sensorIV)
{
  double HexX[MAXVERTICES] = {0.};
  double HexY[MAXVERTICES] = {0.};
  for(int iv = -7; iv < 8; iv++) {
    for(int iu = -7; iu < 8; iu++) {
      if(!IsCellValid.iu_iv_valid(layerID, sensorIU, sensorIV, iu, iv, m_sensorsize)) continue;
      CellXY = TheCell.GetCellCoordinatesForPlots(layerID, sensorIU, sensorIV, iu, iv, m_sensorsize);
      assert(CellXY.size() == 4 || CellXY.size() == 6);
      unsigned int iVertex = 0;
      for(std::vector<std::pair<double, double>>::const_iterator it = CellXY.begin(); it != CellXY.end(); it++) {
	HexX[iVertex] =  it->first;
	HexY[iVertex] =  it->second;
	++iVertex;
      }
      //Somehow cloning of the TH2Poly was not working. Need to look at it. Currently physically booked another one.
      poly.AddBin(CellXY.size(), HexX, HexY);
    }//loop over iu
  }//loop over iv
}


void RawHitPlotter::endJob()
{
  usesResource("TFileService");
  edm::Service<TFileService> fs;
  TFileDirectory dir = fs->mkdir( "HexagonalPlotter" );
  std::map<int,TH2Poly*>  pedPolyMap;
  std::map<int,TH2Poly*>  pedPolyMapNC;
  std::map<int,TH2Poly*>  noisePolyMap;
  std::map<int,TH2Poly*>  noisePolyMapNC;
  std::map<int,TH2Poly*>  chanMap;
  std::ostringstream os( std::ostringstream::ate );
  TH2Poly *h;
  for(size_t ib = 0; ib<N_HEXABOARDS; ib++) {
    for( size_t it=0; it<NUMBER_OF_TIME_SAMPLES-0; it++ ){
      h=dir.make<TH2Poly>();
      os.str("");
      os<<"HighGain_HexaBoard"<<ib<<"_TimeSample"<<it;
      h->SetName(os.str().c_str());
      h->SetTitle(os.str().c_str());
      InitTH2Poly(*h, (int)ib, 0, 0);
      pedPolyMap.insert( std::pair<int,TH2Poly*>(100*ib+it,h) );
      h=dir.make<TH2Poly>();
      os.str("");
      os<<"NC_HighGain_HexaBoard"<<ib<<"_TimeSample"<<it;
      h->SetName(os.str().c_str());
      h->SetTitle(os.str().c_str());
      InitTH2Poly(*h, (int)ib, 0, 0);
      pedPolyMapNC.insert( std::pair<int,TH2Poly*>(100*ib+it,h) );
      h=dir.make<TH2Poly>();
      os.str("");
      os<<"Noise_HighGain_HexaBoard"<<ib<<"_TimeSample"<<it;
      h->SetName(os.str().c_str());
      h->SetTitle(os.str().c_str());
      InitTH2Poly(*h, (int)ib, 0, 0);
      noisePolyMap.insert( std::pair<int,TH2Poly*>(100*ib+it,h) );
      h=dir.make<TH2Poly>();
      os.str("");
      os<<"NC_Noise_HighGain_HexaBoard"<<ib<<"_TimeSample"<<it;
      h->SetName(os.str().c_str());
      h->SetTitle(os.str().c_str());
      InitTH2Poly(*h, (int)ib, 0, 0);
      noisePolyMapNC.insert( std::pair<int,TH2Poly*>(100*ib+it,h) );
    }
  }

  for( std::set< std::pair<int,HGCalTBDetId> >::iterator it=setOfConnectedDetId.begin(); it!=setOfConnectedDetId.end(); ++it ){
    int iboard=(*it).first/1000;
    int iski=((*it).first%1000)/100;
    int ichan=(*it).first%100;
    int ichanNC=(*it).first%100+1;
    HGCalTBDetId detid=(*it).second;
    CellCentreXY = TheCell.GetCellCentreCoordinatesForPlots( detid.layer(), detid.sensorIU(), detid.sensorIV(), detid.iu(), detid.iv(), m_sensorsize );
    double iux = (CellCentreXY.first < 0 ) ? (CellCentreXY.first + delta) : (CellCentreXY.first - delta) ;
    double iuy = (CellCentreXY.second < 0 ) ? (CellCentreXY.second + delta) : (CellCentreXY.second - delta);
    for( size_t it=0; it<NUMBER_OF_TIME_SAMPLES-0; it++ ){
      pedPolyMap[ 100*iboard+it ]->Fill(iux/2 , iuy, m_h_adcHigh[iboard*100000+iski*10000+ichan*100+it]->GetMean() );
      pedPolyMapNC[ 100*iboard+it ]->Fill(iux/2 , iuy, m_h_adcHigh[iboard*100000+iski*10000+ichanNC*100+it]->GetMean() );
      noisePolyMap[ 100*iboard+it ]->Fill(iux/2 , iuy, m_h_adcHigh[iboard*100000+iski*10000+ichan*100+it]->GetRMS() );
      noisePolyMapNC[ 100*iboard+it ]->Fill(iux/2 , iuy, m_h_adcHigh[iboard*100000+iski*10000+ichanNC*100+it]->GetRMS() );
    }
  }
}

void RawHitPlotter::fillDescriptions(edm::ConfigurationDescriptions& descriptions)
{
  edm::ParameterSetDescription desc;
  desc.setUnknown();
  descriptions.addDefault(desc);
}

DEFINE_FWK_MODULE(RawHitPlotter);