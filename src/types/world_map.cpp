#include "world_map.h"

#include <fstream>

namespace proslam {
  using namespace srrg_core;

  WorldMap::WorldMap() {
    std::cerr << "WorldMap::WorldMap|constructing" << std::endl;
    _frame_queue_for_local_map.clear();
    _landmarks.clear();
    _frames.clear();
    _local_maps.clear();
    _currently_tracked_landmarks.clear();
    std::cerr << "WorldMap::WorldMap|constructed" << std::endl;
  };

  WorldMap::~WorldMap() {
    std::cerr << "WorldMap::WorldMap|destroying" << std::endl;

    //ds free landmarks
    for(LandmarkPointerMap::iterator it = _landmarks.begin(); it != _landmarks.end(); ++it) {
      delete it->second;
    }

    //ds free all frames (also destroys local maps by their anchors)
    for(FramePointerMap::iterator it = _frames.begin(); it != _frames.end(); ++it) {
      delete it->second;
    }
    std::cerr << "WorldMap::WorldMap|destroyed" << std::endl;
  }
  
  Frame* WorldMap::createFrame(const TransformMatrix3D& frame_to_world_guess_, const real& maximum_depth_close_){
    if (_previous_frame) {
      _previous_frame->releaseImages();
    }
    _previous_frame = _current_frame;
    _current_frame  = new Frame(this, _previous_frame, 0, frame_to_world_guess_, maximum_depth_close_);

    if (_root_frame == 0) {
      _root_frame = _current_frame;
    }

    if (_previous_frame) {
      _previous_frame->setNext(_current_frame);
    }

    _frames.put(_current_frame);
    _frame_queue_for_local_map.push_back(_current_frame);
    return _current_frame;
  }

  const bool WorldMap::createLocalMap() {
    if (_previous_frame == 0) {
      return false;
    }

    //ds reset closure status
    _relocalized = false;

    //ds update distance traveled and last pose
    const TransformMatrix3D robot_pose_last_to_current = _previous_frame->worldToRobot()*_current_frame->robotToWorld();
    _distance_traveled_window += robot_pose_last_to_current.translation().norm();
    _degrees_rotated_window   += toOrientationRodrigues(robot_pose_last_to_current.linear()).norm();

    //ds check if we can generate a keyframe - if generated by translation only a minimum number of frames in the buffer is required - or a new tracking context
    if (_degrees_rotated_window   > _minimum_degrees_rotated_for_local_map                                                                                 ||
        (_distance_traveled_window > _minimum_distance_traveled_for_local_map && _frame_queue_for_local_map.size() > _minimum_number_of_frames_for_local_map)||
        (_frame_queue_for_local_map.size() > _minimum_number_of_frames_for_local_map && _local_maps.size() < 5)                                              ) {

      //ds create the new keyframe and add it to the keyframe database
      _current_local_map = new LocalMap(_current_frame, _frame_queue_for_local_map);
      _local_maps.push_back(_current_local_map);

      //ds reset generation properties
      resetWindowForLocalMapCreation();

      //ds current frame is now center of a local map - update structures
      _current_frame = _current_local_map;
      _frames.replace(_current_frame);

      //ds local map generated
      return true;
    } else {

      //ds no local map generated
      return false;
    }
  }
  
  Landmark* WorldMap::createLandmark(const PointCoordinates& coordinates_in_world_, const FramePoint* origin_){
    Landmark* landmark = new Landmark(coordinates_in_world_, origin_);
    _landmarks.put(landmark);
    return landmark;
  }

  void WorldMap::closeLocalMaps(LocalMap* query_,
                                       const LocalMap* reference_,
                                       const TransformMatrix3D& transform_query_to_reference_) {
    query_->add(reference_, transform_query_to_reference_);
    _relocalized = true;

    //ds informative only
    ++_number_of_closures;
  }

  void WorldMap::resetWindowForLocalMapCreation() {
    _distance_traveled_window = 0;
    _degrees_rotated_window   = 0;

    //ds free memory if desired (saves a lot of memory costs a little computation)
    if (_drop_framepoints) {

      //ds the last frame well need for the next tracking step
      _frame_queue_for_local_map.pop_back();

      //ds purge the rest
      for (Frame* frame: _frame_queue_for_local_map) {
        frame->releasePoints();
      }
    }
    _frame_queue_for_local_map.clear();

    //ds allocate a clean new landmarks map
    LandmarkPointerMap landmarks_cleaned;
    landmarks_cleaned.clear();

    //ds also clean up unused landmarks - first free memory for unused landmarks
    for(LandmarkPointerMap::iterator it = _landmarks.begin(); it != _landmarks.end(); ++it) {

      //ds if the landmark is not part of a local map and is not currently tracked
      if (it->second->localMap() == 0 && !it->second->isCurrentlyTracked()) {

        //ds free it
        delete it->second;
      } else {

        //ds keep the landmark
        landmarks_cleaned.put(it->second);
      }
    }

    //ds clear reference
    _landmarks.clear();

    //ds and reassign
    _landmarks.swap(landmarks_cleaned);
  }

  //ds dump trajectory to file (in KITTI benchmark format only for now)
  void WorldMap::writeTrajectory(const std::string& filename_) const {

    //ds construct filename
    std::string filename(filename_);

    //ds if not set
    if (filename_ == "") {

      //ds generate generic filename with timestamp
      filename = "trajectory-"+std::to_string(static_cast<uint64_t>(std::round(srrg_core::getTime())))+".txt";
    }

    //ds open file stream (overwriting)
    std::ofstream outfile_trajectory(filename, std::ifstream::out);
    assert(outfile_trajectory.good());

    //ds for each frame (assuming continuous, sequential indexing)
    for (Index index_frame = 0; index_frame < _frames.size(); ++index_frame) {

      //ds buffer transform
      const TransformMatrix3D& robot_to_world = _frames.at(index_frame)->robotToWorld();

      //ds dump transform according to KITTI format
      for (uint8_t u = 0; u < 3; ++u) {
        for (uint8_t v = 0; v < 4; ++v) {
          outfile_trajectory << robot_to_world(u,v) << " ";
        }
      }
      outfile_trajectory << "\n";
    }
    outfile_trajectory.close();
    std::cerr << "WorldMap::WorldMap|saved trajectory to: " << filename << std::endl;
  }
}

