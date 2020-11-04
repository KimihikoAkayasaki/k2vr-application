#include "K2ServerDriver.h"
#include <thread>
#include <boost/lexical_cast.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>

int K2ServerDriver::init_ServerDriver(std::string const& port)
{
	using namespace std::chrono_literals;
	_isActive = true;

	// initialize the zmq context with a single IO thread
	try
	{
		socket.bind(port);
	}
	catch (std::exception const& e)
	{
		return -1;
	}

	std::thread([&]()
	{
		// create chrono for limiting loop timing
		using clock = std::chrono::steady_clock;
		auto next_frame = clock::now();

		// run until termination
		while (true)
		{
			while (_isActive)
			{
				// measure loop time, let's run at 140/s
				next_frame += std::chrono::milliseconds(1000 / 140);

				// create new ZMQ request to receive
				zmq::message_t request;
				// receive a request from client
				socket.recv(&request);
				// parse request, send reply and return
				parse_message(request.msg_str());

				//Sleep until next frame, if time haven't passed yet
				std::this_thread::sleep_until(next_frame);
			}
			// if we're currently not running, just wait
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
	}).detach();

	// if everything went ok, return 0
	return 0;
}

void K2ServerDriver::parse_message(std::string message)
{
	std::string _reply; // Reply that will be sent to client
	bool isReplying = false;
	const std::string cmd("/C"), param("/P"), param1("/P1"), term("/T");
	const std::string _data = message.substr(0, message.rfind(term));

	if (!_data.empty())
	{
		const std::string _command = _data.substr(_data.find(cmd) + cmd.length(), _data.rfind(param) - param.length()),
		                  _parameters = _data.substr(_data.find(param) + param.length());

		// We'll use /P1 switch only with certain commands,
		// so don't care about it now

		// Add new tracker to server
		if (_command == "ADD_TRACKER")
		{
			isReplying = true; // We're reply to this request

			// Create input archive to read parameters
			std::istringstream ifs(_parameters);
			boost::archive::text_iarchive ia(ifs);

			// Read parameters (tracker base)
			K2Objects::K2TrackerBase _tracker;
			ia >> _tracker;

			// Push new tracker to vector and return its id
			trackerVector.emplace_back(K2Tracker(_tracker));
			_reply = std::to_string(trackerVector.size() - 1);
		}
			// Set all trackers' state
		else if (_command == "SET_STATE_ALL")
		{
			isReplying = true; // We're reply to this request
			_reply = "0"; // Assume operation failed

			// Construct bool variable from parameter
			bool _state = boost::lexical_cast<bool>(_parameters);

			// Set all trackers' state
			for (K2Tracker& k2_tracker : trackerVector)
			{
				if (!k2_tracker.is_added()) k2_tracker.spawn(); // spawn if needed
				k2_tracker.set_state(_state); // set state
			}
			_reply = "1"; // If success, return true
		}

		// Rest of commands will need /P1 switch
		std::string _parameter0 = _parameters.substr(0, _parameters.rfind(param1)),
		            _parameter1 = _parameters.substr(_parameters.rfind(param1) + param1.length());

		// Parse only if found more parameters
		if (!_parameter0.empty() && !_parameter1.empty())
		{
			// Set one tracker's state
			if (_command == "SET_STATE")
			{
				isReplying = true; // We're reply to this request
				_reply = "0"; // Assume operation failed

				// Construct bool variable from first parameter
				int _id = boost::lexical_cast<int>(_parameter0);

				// Construct bool variable from second parameter
				bool _state = boost::lexical_cast<bool>(_parameter1);

				try
				{
					// Set tracker's state to one gathered from argument
					trackerVector.at(_id).set_state(_state);
					_reply = "1"; // If success, return true
				}
				catch (...)
				{
				}
			}
				// Update one tracker's pose
			else if (_command == "UPDATE_POSE")
			{
				// Construct bool variable from first parameter
				int _id = boost::lexical_cast<int>(_parameter0);

				// Create input archive from second parameter
				std::istringstream ifs(_parameters);
				boost::archive::text_iarchive ia(ifs);

				// Read parameters (tracker base)
				K2Objects::K2PosePacket _pose_packet;
				ia >> _pose_packet;

				try
				{
					// Update tracker pose (with time offset)
					if (int(_pose_packet.millisFromNow) != 0)
					{
						trackerVector.at(_id).set_pose(_pose_packet, _pose_packet.millisFromNow);
					}
					else
					{
						// If there is no time offset, just update
						trackerVector.at(_id).set_pose(_pose_packet);
					}
				}
				catch (...)
				{
				}
			}
				// Update one tracker's data: only if it's not initialized yet
			else if (_command == "UPDATE_DATA")
			{
				// Construct bool variable from first parameter
				int _id = boost::lexical_cast<int>(_parameter0);

				// Create input archive from second parameter
				std::istringstream ifs(_parameter1);
				boost::archive::text_iarchive ia(ifs);

				// Read parameters (tracker base)
				K2Objects::K2DataPacket _data_packet;
				ia >> _data_packet;

				try
				{
					// Update tracker pose (with time offset)
					if (int(_data_packet.millisFromNow) != 0)
					{
						trackerVector.at(_id).set_data(_data_packet, _data_packet.millisFromNow);
					}
					else
					{
						// If there is no time offset, just update
						trackerVector.at(_id).set_data(_data_packet);
					}
				}
				catch (...)
				{
				}
			}
		}

		if (isReplying)
		{
			// send the reply to the client
			zmq::message_t reply(_reply.size());
			std::memcpy(reply.data(), _reply.data(), _reply.size());
			socket.send(reply);
		}
	}
}

/*
 * FROM-TO
 * BEGIN-STRING_END: data.substr(data.rfind(begin) + begin.length())
 * STRING_BEGIN-END: data.substr(0, data.rfind(end))
 * BEGIN_END: data.substr(data.find(begin) + begin.length(), data.rfind(end) - end.length())
 */