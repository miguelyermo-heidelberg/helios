#include "OscillatingMirrorBeamDeflector.h"

#include <iostream>
#include <sstream>
using namespace std;

#define _USE_MATH_DEFINES
#include <math.h>
#include <logging.hpp>

#include "maths/Directions.h"
#include "MathConverter.h"

// ***  CONSTRUCTION / DESTRUCTION  *** //
// ************************************ //
std::shared_ptr<AbstractBeamDeflector> OscillatingMirrorBeamDeflector::clone(){
    std::shared_ptr<AbstractBeamDeflector> ombd =
        std::make_shared<OscillatingMirrorBeamDeflector>(
            OscillatingMirrorBeamDeflector(
                cfg_device_scanAngleMax_rad,
                cfg_device_scanFreqMax_Hz,
                cfg_device_scanFreqMin_Hz,
                cfg_device_scanProduct
            )
        );
    _clone(ombd);
    return ombd;
}
void OscillatingMirrorBeamDeflector::_clone(
    std::shared_ptr<AbstractBeamDeflector> abd
){
    AbstractBeamDeflector::_clone(abd);
    OscillatingMirrorBeamDeflector *ombd = (OscillatingMirrorBeamDeflector *)
        abd.get();
    ombd->cfg_device_scanProduct = cfg_device_scanProduct;
    ombd->currentScanLinePulse = currentScanLinePulse;
    ombd->cached_pulsesPerScanline = cached_pulsesPerScanline;
}

// ***  M E T H O D S  *** //
// *********************** //
void OscillatingMirrorBeamDeflector::applySettings(std::shared_ptr<ScannerSettings> settings) {
	AbstractBeamDeflector::applySettings(settings);
	cached_angleBetweenPulses_rad = (double)(this->cfg_setting_scanFreq_Hz * this->cfg_setting_scanAngle_rad * 4) / settings->pulseFreq_Hz;
	cached_pulsesPerScanline = (int)(((double)settings->pulseFreq_Hz) / this->cfg_setting_scanFreq_Hz);
}

void OscillatingMirrorBeamDeflector::doSimStep() {
	currentScanLinePulse++;

	if (currentScanLinePulse == cached_pulsesPerScanline) {
		currentScanLinePulse = 0;
	}

	// Update beam angle:
	int bla = std::min(currentScanLinePulse, cached_pulsesPerScanline / 2) - std::max(0, currentScanLinePulse - cached_pulsesPerScanline / 2);

	state_currentBeamAngle_rad = -this->cfg_setting_scanAngle_rad + cached_angleBetweenPulses_rad * bla;

	// Rotate to current position:
	this->cached_emitterRelativeAttitude = Rotation(Directions::right, state_currentBeamAngle_rad);

}

void OscillatingMirrorBeamDeflector::setScanAngle_rad(double scanAngle_rad) {
	double scanAngle_deg = MathConverter::radiansToDegrees(scanAngle_rad);

	// Max. scan angle is limited by scan product:
	if (scanAngle_deg * this->cfg_setting_scanFreq_Hz > this->cfg_device_scanProduct) {
		logging::WARN(
		    "ERROR: Requested scan angle exceeds device limitations "
            "as defined by scan product. "
            "Will set it to maximal possible value."
        );
		scanAngle_deg = ((double) this->cfg_device_scanProduct) / this->cfg_setting_scanFreq_Hz;
	}

	this->cfg_setting_scanAngle_rad = MathConverter::degreesToRadians(scanAngle_deg);
	stringstream ss;
	ss << "Scan angle set to " << scanAngle_deg << " degrees.";
	logging::INFO(ss.str());
}

void OscillatingMirrorBeamDeflector::setScanFreq_Hz(double scanFreq_Hz) {
	// Max. scan frequency is limited by scan product:
	if( MathConverter::radiansToDegrees(this->cfg_setting_scanAngle_rad) *
	    scanFreq_Hz > this->cfg_device_scanProduct
    ){
		logging::WARN(
		    "ERROR: Requested scan frequency exceeds device limitations "
            "as defined by scan product. "
            "Will set it to maximal possible value."
        );
		scanFreq_Hz = ((double) this->cfg_device_scanProduct) /
		    MathConverter::radiansToDegrees(this->cfg_setting_scanAngle_rad);
	}
	this->cfg_setting_scanFreq_Hz = scanFreq_Hz;
	stringstream ss;
	ss << "Scan frequency set to " << this->cfg_setting_scanFreq_Hz << " Hz.";
	logging::INFO(ss.str());
}