#include "KinectV2Handler.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <Ole2.h>

#include <ppl.h>
#include <thread>
#include <chrono>

HRESULT KinectV2Handler::getStatusResult()
{
	BOOLEAN avail;
	kinectSensor->get_IsAvailable(&avail);
	if (avail)
		return S_OK;
	return S_FALSE;
	// Hresult only actually determines whether the function worked, the bool is the true value....
}

std::string KinectV2Handler::statusResultString(HRESULT stat)
{
	switch (stat)
	{
	case S_OK: return "S_OK";
	case S_FALSE: return "Sensor Unavailable! Check if it's plugged in to your USB and power plugs";
	default: return "Uh Oh undefined kinect error! " + std::to_string(stat);
	}
}

void KinectV2Handler::initialize()
{
	try
	{
		initialized = initKinect();
		// initializeColor();
		// Commented both image frames out, as most people use the kinect for skeletal data
		// Updating all of the arrays uses a shit ton of CPU, but then again, it's still WIP
		// initializeDepth();
		initializeSkeleton();
		if (!initialized) throw FailedKinectInitialisation;
	}
	catch (std::exception& e)
	{
	}
}

void KinectV2Handler::initializeSkeleton()
{
	if (bodyFrameReader)
		bodyFrameReader->Release();
	IBodyFrameSource* bodyFrameSource;
	kinectSensor->get_BodyFrameSource(&bodyFrameSource);
	bodyFrameSource->OpenReader(&bodyFrameReader);

	// Newfangled event based frame capture
	// https://github.com/StevenHickson/PCL_Kinect2SDK/blob/master/src/Microsoft_grabber2.cpp
	h_bodyFrameEvent = (WAITABLE_HANDLE)CreateEvent(nullptr, FALSE, FALSE, nullptr);
	HRESULT hr = bodyFrameReader->SubscribeFrameArrived(&h_bodyFrameEvent);
	if (bodyFrameSource) bodyFrameSource->Release();
}

void KinectV2Handler::terminateSkeleton()
{
	if (bodyFrameReader)
	{
		HRESULT hr = bodyFrameReader->UnsubscribeFrameArrived(h_bodyFrameEvent);
		if (FAILED(hr))
		{
			throw std::exception("Couldn't unsubscribe frame!");
		}
		CloseHandle((HANDLE)h_bodyFrameEvent);
		h_bodyFrameEvent = NULL;

		bodyFrameReader->Release();
		bodyFrameReader = nullptr;
	}
}

void KinectV2Handler::update()
{
	if (isInitialized())
	{
		BOOLEAN isAvailable = false;
		HRESULT kinectStatus = kinectSensor->get_IsAvailable(&isAvailable);
		if (kinectStatus == S_OK)
		{
			// NEW ARRIVED FRAMES ------------------------
			MSG msg;
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) // Unneccesary?
			{
				DispatchMessage(&msg);
			}

			if (h_bodyFrameEvent)
			{
				//printf("Kinect Event ID: %d\n" ,(int)h_bodyFrameEvent);

				//now check for IR Events
				HANDLE handles[] = {reinterpret_cast<HANDLE>(h_bodyFrameEvent)};
				// , reinterpret_cast<HANDLE>(ke.hMSEvent)		};

				switch (MsgWaitForMultipleObjects(_countof(handles), handles, false, 0, QS_ALLINPUT))
				{
				case WAIT_OBJECT_0:
					{
						IBodyFrameArrivedEventArgs* pArgs = nullptr;
						//printf("Body Frame Event Signaled.\n");

						if (bodyFrameReader)
						{
							HRESULT hr = bodyFrameReader->GetFrameArrivedEventData(h_bodyFrameEvent, &pArgs);
							//printf("Retreive Frame Arrive Event Data -HR: %d\n", hr);

							if (SUCCEEDED(hr))
							{
								//printf("Retreived Frame Arrived Event Data\n");
								onBodyFrameArrived(*bodyFrameReader, *pArgs);
								pArgs->Release();
								//printf("Frame Arrived Event Data Released\n");
							}
						}
					}
					break;
				}
			}
		}
	}
}

void KinectV2Handler::onBodyFrameArrived(IBodyFrameReader& sender, IBodyFrameArrivedEventArgs& eventArgs)
{
	updateSkeletalData();
}

void KinectV2Handler::updateSkeletalData()
{
	if (bodyFrameReader)
	{
		IBodyFrame* bodyFrame = nullptr;
		HRESULT frameReceived = bodyFrameReader->AcquireLatestFrame(&bodyFrame);
		if (FAILED(frameReceived))
		{
			if (frameReceived == E_PENDING)
			{
				// LOG(INFO) << "Could not retrieve skeleton frame, stuck pending...";
			}
		}
		//IBodyFrameReference* frameRef = nullptr;
		//multiFrame->get_BodyFrameReference(&frameRef);
		//frameRef->AcquireFrame(&bodyFrame);
		//if (frameRef) frameRef->Release();
		//if (bodyFrameReader) bodyFrameReader->Release();
		if (!bodyFrame) return;

		bodyFrame->GetAndRefreshBodyData(BODY_COUNT, kinectBodies);
		newBodyFrameArrived = true;
		if (bodyFrame) bodyFrame->Release();

		// We have the frame, now parse it
		updateSkeletalFilters();
	}
}

void KinectV2Handler::updateSkeletalFilters()
{
	for (int i = 0; i < BODY_COUNT; i++)
	{
		if (kinectBodies[i])
			kinectBodies[i]->get_IsTracked(&isTracking);
		if (isTracking)
		{
			isSkeletonTracked = true;
			kinectBodies[i]->GetJoints(JointType_Count, joints);
			kinectBodies[i]->GetJointOrientations(JointType_Count, jointOrientations);

			/* Copy joint positions */
			for (int j = 0; j < ktvr::Joint_Total; ++j)
			{
				TrackingDeviceBase::jointPositions[j].w = 1.0f;
				TrackingDeviceBase::jointPositions[j].x = joints[globalIndex[j]].Position.X;
				TrackingDeviceBase::jointPositions[j].y = joints[globalIndex[j]].Position.Y;
				TrackingDeviceBase::jointPositions[j].z = joints[globalIndex[j]].Position.Z;

				TrackingDeviceBase::trackingStates[j] = joints[globalIndex[j]].TrackingState;
			}

			/* Copy joint orientations */
			for (int k = 0; k < ktvr::Joint_Total; ++k)
			{
				TrackingDeviceBase::jointOrientations[k].w = jointOrientations[globalIndex[k]].Orientation.w;
				TrackingDeviceBase::jointOrientations[k].x = jointOrientations[globalIndex[k]].Orientation.x;
				TrackingDeviceBase::jointOrientations[k].y = jointOrientations[globalIndex[k]].Orientation.y;
				TrackingDeviceBase::jointOrientations[k].z = jointOrientations[globalIndex[k]].Orientation.z;
			}

			newBodyFrameArrived = false;
			break; // Only first skeleton
		}
		isSkeletonTracked = false;
	}
}

bool KinectV2Handler::initKinect()
{
	if (FAILED(GetDefaultKinectSensor(&kinectSensor)))
	{
		return false;
	}
	if (kinectSensor)
	{
		kinectSensor->get_CoordinateMapper(&coordMapper);

		HRESULT hr_open = kinectSensor->Open();
		//kinectSensor->OpenMultiSourceFrameReader( FrameSourceTypes::FrameSourceTypes_Body| FrameSourceTypes::FrameSourceTypes_Depth
		//    | FrameSourceTypes::FrameSourceTypes_Color,
		//   &frameReader);
		//return frameReader;
		std::this_thread::sleep_for(std::chrono::seconds(2));
		// Necessary to allow kinect to become available behind the scenes

		BOOLEAN available = false;
		kinectSensor->get_IsAvailable(&available);

		if (FAILED(hr_open) || !available)
		{
			return false;
		}
		return true;
	}
	return false;
}

void KinectV2Handler::shutdown()
{
	try
	{
		// Release the Kinect sensor, called form k2vr
		// OR from the crash handler
		kinectSensor->Release();
	}
	catch (std::exception& e)
	{
	}
}
