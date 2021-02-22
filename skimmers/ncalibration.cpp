#include <cstdlib>
#include <iostream>
#include <fstream>
#include <cmath>
#include <string>
#include <vector>

#include "TFile.h"
#include "TTree.h"
#include "TVector3.h"
#include "TH2.h"
#include "TH1.h"
#include "TClonesArray.h"

#include "reader.h"
#include "bank.h"

#include "BParticle.h"
#include "BCalorimeter.h"
#include "BScintillator.h"
#include "BBand.h"
#include "BEvent.h"

#include "RCDB/Connection.h"

#include "constants.h"
#include "readhipo_helper.h"

using namespace std;


int main(int argc, char** argv) {
	// check number of arguments
	if( argc < 5 ){
		cerr << "Incorrect number of arugments. Instead use:\n\t./code [outputFile] [MC/DATA] [inputFile] \n\n";
		cerr << "\t\t[outputFile] = ____.root\n";
		cerr << "\t\t[<MC,DATA> = <0, 1> \n";
		cerr << "\t\t[<load shifts N,Y> = <0, 1> \n";
		cerr << "\t\t[inputFile] = ____.hipo ____.hipo ____.hipo ...\n\n";
		return -1;
	}

	int MC_DATA_OPT = atoi(argv[2]);
	int loadshifts_opt = atoi(argv[3]);

	// Create output tree
	TFile * outFile = new TFile(argv[1],"RECREATE");
	TTree * outTree = new TTree("calib","BAND Neutrons and CLAS Electrons");
	//	Event info:
	int Runno		= 0;
	double Ebeam		= 0;
	double gated_charge	= 0;
	double livetime		= 0;
	double starttime	= 0;
	double current		= 0;
	int eventnumber = 0;
	bool goodneutron 	= false;
	int nleadindex 		= -1;
	// 	Neutron info:
	int nMult		= 0;
	TClonesArray * nHits 	= new TClonesArray("bandhit");
	TClonesArray &saveHit 	= *nHits;
	//	Electron info:
	clashit eHit;
	// 	Event branches:
	outTree->Branch("Runno"		,&Runno			);
	outTree->Branch("Ebeam"		,&Ebeam			);
	outTree->Branch("gated_charge"	,&gated_charge		);
	outTree->Branch("livetime"	,&livetime		);
	outTree->Branch("starttime"	,&starttime		);
	outTree->Branch("current"	,&current		);
	outTree->Branch("eventnumber",&eventnumber);
	//	Neutron branches:
	outTree->Branch("nMult"		,&nMult			);
	outTree->Branch("nHits"		,&nHits			);
	//Branches to store if good Neutron event and leadindex
	outTree->Branch("goodneutron"		,&goodneutron	);
	outTree->Branch("nleadindex"		,&nleadindex			);
	//	Electron branches:
	outTree->Branch("eHit"		,&eHit			);

	// Connect to the RCDB
	rcdb::Connection connection("mysql://rcdb@clasdb.jlab.org/rcdb");

	shiftsReader shifts;
	double * FADC_INITBAR;
	double * TDC_INITBAR;
	if( loadshifts_opt ){
		// Load bar shifts
		shifts.LoadInitBarFadc	("../include/FADC_pass1v0_initbar.txt");
		FADC_INITBAR = (double*) shifts.getInitBarFadc();
		shifts.LoadInitBar	("../include/TDC_pass1v0_initbar.txt");
		TDC_INITBAR = (double*) shifts.getInitBar();
		// Load run-by-run shifts
		// 	for 10.2 these are not needed
		//shifts.LoadInitRunFadc("../include/FADC_pass1v0_initrun.txt");
		//FADC_INITRUN = (double*) shifts.getInitRunFadc();
	}
	// Effective velocity for re-doing x- calculation
	double * FADC_EFFVEL_S6200;
	double *  TDC_EFFVEL_S6200;
	double * FADC_EFFVEL_S6291;
	double *  TDC_EFFVEL_S6291;
	double *  FADC_LROFF_S6200;
	double *   TDC_LROFF_S6200;
	double *  FADC_LROFF_S6291;
	double *   TDC_LROFF_S6291;
	shifts.LoadEffVel	("../include/EffVelocities_S6200.txt",	"../include/EffVelocities_S6291.txt");
	shifts.LoadLrOff	("../include/LrOffsets_S6200.txt",	"../include/LrOffsets_S6291.txt");
	FADC_EFFVEL_S6200	= (double*) shifts.getFadcEffVel(6200);
	TDC_EFFVEL_S6200	= (double*)  shifts.getTdcEffVel(6200);
	FADC_EFFVEL_S6291	= (double*) shifts.getFadcEffVel(6291);
	TDC_EFFVEL_S6291	= (double*)  shifts.getTdcEffVel(6291);

	FADC_LROFF_S6200	= (double*) shifts.getFadcLrOff(6200);
	TDC_LROFF_S6200		= (double*)  shifts.getTdcLrOff(6200);
	FADC_LROFF_S6291	= (double*) shifts.getFadcLrOff(6291);
	TDC_LROFF_S6291		= (double*)  shifts.getTdcLrOff(6291);

	//Maps for geometry positions
	std::map<int,double> bar_pos_y;
	std::map<int,double> bar_pos_z;
	//Load geometry position of bars
	getBANDBarGeometry("../include/band-bar-geometry.txt", bar_pos_y,bar_pos_z);

	// Load input file
	for( int i = 4 ; i < argc ; i++ ){
		if( MC_DATA_OPT == 0){
			int runNum = 11;
			Runno = runNum;
		}
		else if( MC_DATA_OPT == 1){
			int runNum = getRunNumber(argv[i]);
			Runno = runNum;
			auto cnd = connection.GetCondition(runNum, "beam_energy");
			Ebeam = cnd->ToDouble() / 1000.; // [GeV]
			Ebeam = 4.244;
			current = connection.GetCondition( runNum, "beam_current") ->ToDouble(); // [nA]
		}
		else{
			exit(-1);
		}

		// Setup hipo reading for this file
		TString inputFile = argv[i];
		hipo::reader reader;
		reader.open(inputFile);
		hipo::dictionary  factory;
		hipo::schema	  schema;
		reader.readDictionary(factory);
		BEvent		event_info		(factory.getSchema("REC::Event"		));
		BBand		band_hits		(factory.getSchema("BAND::hits"		));
		hipo::bank	scaler			(factory.getSchema("RUN::scaler"	));
		hipo::bank  run_config (factory.getSchema("RUN::config"));
		hipo::bank      DC_Track                (factory.getSchema("REC::Track"         ));
		hipo::bank      DC_Traj                 (factory.getSchema("REC::Traj"          ));
		hipo::event 	readevent;
		hipo::bank	band_rawhits		(factory.getSchema("BAND::rawhits"	));
		hipo::bank	band_adc		(factory.getSchema("BAND::adc"		));
		hipo::bank	band_tdc		(factory.getSchema("BAND::tdc"		));
		BParticle	particles		(factory.getSchema("REC::Particle"	));
		BCalorimeter	calorimeter		(factory.getSchema("REC::Calorimeter"	));
		BScintillator	scintillator		(factory.getSchema("REC::Scintillator"	));


		// Loop over all events in file
		int event_counter = 0;
		gated_charge = 0;
		livetime	= 0;

		int n_one = 0;
		int n_two = 0;
		int n_thr = 0;
		int n_mor = 0;
		while(reader.next()==true){
			// Clear all branches
			gated_charge	= 0;
			livetime	= 0;
			starttime 	= 0;
			eventnumber = 0;
			// Neutron
			nMult		= 0;
			nleadindex = -1;
			goodneutron = false;
			bandhit nHit[maxNeutrons];
			nHits->Clear();
			// Electron
			eHit.Clear();


			// Count events
			if(event_counter%10000==0) cout << "event: " << event_counter << endl;
			event_counter++;

			// Load data structure for this event:
			reader.read(readevent);
			readevent.getStructure(event_info);
			readevent.getStructure(scaler);
			readevent.getStructure(run_config);
			// band struct
			readevent.getStructure(band_hits);
			readevent.getStructure(band_rawhits);
			readevent.getStructure(band_adc);
			readevent.getStructure(band_tdc);
			// electron struct
			readevent.getStructure(particles);
			readevent.getStructure(calorimeter);
			readevent.getStructure(scintillator);
			readevent.getStructure(DC_Track);
			readevent.getStructure(DC_Traj);

			//Get Event number from RUN::config
			eventnumber = run_config.getInt( 1 , 0 );

			// Get integrated charge, livetime and start-time from REC::Event
			if( event_info.getRows() == 0 ) continue;
			getEventInfo( event_info, gated_charge, livetime, starttime );

			// Skim the event so we only have a single electron and NO other particles:
			int nElectrons = 0;
			int nOthers = 0;
			for( int part = 0 ; part < particles.getRows() ; part++ ){
				int PID 	= particles.getPid(part);
				int charge 	= particles.getCharge(part);
				if( PID != 11 ) continue;
				if( charge !=-1 ) continue;
				TVector3	momentum = particles.getV3P(part);
				TVector3	vertex	 = particles.getV3v(part);
				TVector3 	beamVec(0,0,Ebeam);
				TVector3	qVec; qVec = beamVec - momentum;
				double EoP=	calorimeter.getTotE(part) /  momentum.Mag();
				double Epcal=	calorimeter.getPcalE(part);
				if( EoP < 0.17 || EoP > 0.3 	) continue;
				if( Epcal < 0.07		) continue;
				if( vertex.Z() < -8 || vertex.Z() > 3 )	continue;
				if( momentum.Mag() < 1.3 || momentum.Mag() > Ebeam)	continue;
				double lV=	calorimeter.getLV(part);
				double lW=	calorimeter.getLW(part);
				if( lV < 15 || lW < 15		) continue;
				double Omega	=Ebeam - sqrt( pow(momentum.Mag(),2) + mE*mE )	;
				double Q2	=	qVec.Mag()*qVec.Mag() - pow(Omega,2)	;
				double W2	=	mP*mP - Q2 + 2.*Omega*mP	;
				//if( Q2 < 2 || Q2 > 10			) continue;
				//if( W2 < 2*2				) continue;

				nElectrons++;
			}
			if( nElectrons == 1 )	n_one++;
			if( nElectrons == 2 )	n_two++;
			if( nElectrons == 3 )	n_thr++;
			if( nElectrons > 3  )	n_mor++;
			if( nElectrons != 1 ) 	continue;
			//if( nOthers != 0 )	continue;


			// Grab the electron information:
			getElectronInfo( particles , calorimeter , scintillator , DC_Track, DC_Traj, eHit , starttime , Runno , Ebeam );

			// Further skim the event so that the trigger electron passes basic fiducial requirements
			if( eHit.getPID() != 11 					) continue;
			if( eHit.getCharge() != -1					) continue;
			if( eHit.getEoP() < 0.17 || eHit.getEoP() > 0.3			) continue;
			if( eHit.getEpcal() < 0.07					) continue;
			if( eHit.getV() < 15 || eHit.getW() < 15			) continue;
			if( eHit.getVtz() < -8 || eHit.getVtz() > 3			) continue;
			if( eHit.getMomentum() < 1.3 || eHit.getMomentum() > 10.6		) continue;
			//if( eHit.getQ2() < 2 || eHit.getQ2() > 10			) continue;
			//if( eHit.getW2() < 2*2						) continue;

			// Grab the neutron information:
			if( MC_DATA_OPT == 0 ){
				getNeutronInfo( band_hits, band_rawhits, band_adc, band_tdc, nMult, nHit , starttime , Runno, bar_pos_y, bar_pos_z );
			}
			else{
				getNeutronInfo( band_hits, band_rawhits, band_adc, band_tdc, nMult, nHit , starttime , Runno, bar_pos_y, bar_pos_z,
						1, 	FADC_LROFF_S6200,	TDC_LROFF_S6200,
							FADC_LROFF_S6291,	TDC_LROFF_S6291,
							FADC_EFFVEL_S6200,	TDC_EFFVEL_S6200,
							FADC_EFFVEL_S6291,	TDC_EFFVEL_S6291	);
			}
			if( loadshifts_opt ){
				for( int n = 0 ; n < nMult ; n++ ){
					nHit[n].setTofFadc(	nHit[n].getTofFadc() 	- FADC_INITBAR[(int)nHit[n].getBarID()] );
					nHit[n].setTof(		nHit[n].getTof() 	- TDC_INITBAR[(int)nHit[n].getBarID()]  );
				}
			}

			// Store the neutrons in TClonesArray
			for( int n = 0 ; n < nMult ; n++ ){
				new(saveHit[n]) bandhit;
				saveHit[n] = &nHit[n];
			}

			if (nMult == 1) {
				goodneutron =  true;
				nleadindex = 0;
			}
			//If nMult > 1: Take nHit and check if good event and give back leading hit index and boolean
			if (nMult > 1) {
				//pass Nhit array, multiplicity and reference to leadindex which will be modified by function
				goodneutron = goodNeutronEvent(nHit, nMult, nleadindex, MC_DATA_OPT);
			}

			// Fill tree based on d(e,e'n)X for data
			if( (nMult == 1 || (nMult > 1 && goodneutron) )&& MC_DATA_OPT == 1 ){
				outTree->Fill();
			} // else fill tree on d(e,e')nX for MC
			else if( MC_DATA_OPT == 0 ){
				outTree->Fill();
			}



		} // end loop over events
	}// end loop over files

	outFile->cd();
	outTree->Write();
	outFile->Close();

	return 0;
}
