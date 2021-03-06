#include "clashit.h"

ClassImp(clashit);

clashit::clashit(){}	// Empty constructor
clashit::~clashit(){}	// Empty destructor

void clashit::Clear(){
	Sector		= 0;
	PID		= 0;
	Charge		= 0;
	Status		= 0;

	Time		= 0;
	Beta		= 0;
	Chi2		= 0;
	Etot		= 0;
	Epcal		= 0;
	Eecin		= 0;
	Eecout		= 0;
	EoP		= 0;
	TimeScint	= 0;
	PathScint	= 0;
	U		= 0;
	V		= 0;
	W		= 0;
	Vtx		= 0;
	Vty		= 0;
	Vtz		= 0;

	Momentum	= 0;
	Theta		= 0;
	Phi		= 0;

	Q		= 0;
	ThetaQ		= 0;
	PhiQ		= 0;

	Q2		= 0;
	Omega		= 0;
	Xb		= 0;
	W2		= 0;

	DC_chi2         = -999;
	DC_NDF          = -999;
	DC_sector       = -999;

	DC_x1           = -999;
	DC_y1           = -999;
	DC_z1           = -999;

	DC_x2           = -999;
	DC_y2           = -999;
	DC_z2           = -999;

	DC_x3           = -999;
	DC_y3           = -999;
	DC_z3           = -999;
}
