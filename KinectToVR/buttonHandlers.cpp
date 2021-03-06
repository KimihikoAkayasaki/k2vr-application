#pragma once
#include <buttonHandlers.h>
#include <KinectToVR.h>

signalHandler::signalHandler(QObject* parent) : QObject(parent)
{
}

/* Process button click, like claibrate, showoffsets, etc. */
void signalHandler::cppSlot(const QString& msg)
{
	std::string str = "Called the C++ slot with message: " + msg.toStdString() + '\n';
	//VSDebug(str.c_str());
	LOG(INFO) << str;

	/* Parse only if we finished set-up to omit dummy messages */
	if (process.started)
	{
		if (msg == "TRACKERSCONNECTCHANGED")
			process.initialized = !process.initialized;

		if (msg == "FLIPCHANGED")
			kinectSettings.flipSkeleton = !kinectSettings.flipSkeleton;

		if (msg == "SHOWOFFSETS")
		{
			process.positionalOffsetsBackup = kinectSettings.positionalOffsets;
			process.orientationOffsetsBackup = kinectSettings.orientationOffsets;
			process.settingOffsets = true;
		}

		if (msg == "OFFSETSCANCELLED")
		{
			process.settingOffsets = false;
			kinectSettings.positionalOffsets = process.positionalOffsetsBackup;
			kinectSettings.orientationOffsets = process.orientationOffsetsBackup;

			/* Update spinboxes to their previous state, cause offsets SET was cancelled */
			updateQSpinboxes(kinectSettings.positionalOffsets, kinectSettings.orientationOffsets);
		}

		if (msg == "OFFSETSAPPROVED")
		{
			process.settingOffsets = false;

			/* only SET because offsets are being updated at execution time */
			process.positionalOffsetsBackup = kinectSettings.positionalOffsets;
			process.orientationOffsetsBackup = kinectSettings.orientationOffsets;
		}

		if (msg == "AUTOCALIBRATION_STARTED")
		{
			/* Start automatic calibration */
			startCalibration();
		}

		if(msg=="START_MANUALCALIB")
		{
			/* Start manual calibration */
			startCalibration(false);
		}

		if (msg == "MANUALCALIBRATION_CANCELLED" || msg == "AUTOCALIBRATION_ABORTED")
		{
			/* Recover calibration values from backup */
			abortCalibration = true;
		}

		if (msg == "INSTALL_LATER")
		{
			/* Popup is already closed */
		}

		if (msg == "INSTALL_NOW")
		{
			/* Close the window and call the installer */
		}

		kinectSettings.saveSettings();
	}
}

/* Process filter change of button click with extra arguments */
void signalHandler::multiCpp(const QString& msg, const QString& arg)
{
	std::string str = "Called the C++ slot with message: " + msg.toStdString() + ", with arguments: " + arg.
		toStdString() + '\n';
	//VSDebug(str.c_str());
	LOG(INFO) << str;

	/* Parse only if we finished set-up to omit dummy messages */
	if (process.started)
	{
		if (msg == "HIPS") {
			if (arg == " Device-inferred Rotation")
				kinectSettings.waistOrientationTrackingOption = k_EnableWaistOrientationFilter;
			else if (arg == " Follow Headset")
				kinectSettings.waistOrientationTrackingOption = k_EnableWaistOrientationFilter_UseHeadOrientation;
			else if (arg == " Disable Rotation")
				kinectSettings.waistOrientationTrackingOption = k_DisableWaistOrientationFilter;
		}

		if (msg == "FEET") {
			if (arg == " Device-inferred Rotation")
				kinectSettings.feetOrientationTrackingOption = k_EnableFeetOrientationFilter;
			else if (arg == " Follow Headset")
				kinectSettings.feetOrientationTrackingOption = k_EnableFeetOrientationFilter_UseHeadOrientation;
			else if (arg == " Disable Rotation")
				kinectSettings.feetOrientationTrackingOption = k_DisableFeetOrientationFilter;
		}

		if (msg == "FILTER") {
			if (arg == " Linear Interpolation")
				kinectSettings.positionalTrackingFilterOption = k_EnableTrackingFilter_LERP;
			else if (arg == " Lowpass Optical")
				kinectSettings.positionalTrackingFilterOption = k_EnableTrackingFilter_LowPass;
			else if (arg == " Extended Kalman")
				kinectSettings.positionalTrackingFilterOption = k_EnableTrackingFilter_Kalman;
			else if (arg == " Disable Filter")
				kinectSettings.positionalTrackingFilterOption = k_DisableTrackingFilter;
		}


		kinectSettings.saveSettings();
	}
}
