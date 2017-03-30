#include "frame.h"
#include "local_map.h"

namespace proslam {

  //ds inner instance count - incremented upon constructor call (also unsuccessful calls)
  Count Frame::_instances = 0;

  Frame::Frame(const WorldMap* context_,
               Frame* previous_,
               Frame* next_,
               const TransformMatrix3D& robot_to_world_,
               const real& maximum_depth_close_): _identifier(_instances),
                                                  _previous(previous_),
                                                  _next(next_),
                                                  _maximum_depth_close(maximum_depth_close_) {
    ++_instances;
    setRobotToWorld(robot_to_world_);
    _points.clear();
  }

  //ds deep clone constructor, used for local map generation - without incrementing the identifier!
  Frame::Frame(Frame* frame_): _identifier(frame_->identifier()),
                               _status(frame_->status()),
                               _previous(frame_->previous()),
                               _next(frame_->next()),
                               _maximum_depth_close(frame_->maximumDepthClose()),
                               _local_map(frame_->localMap()) {

    //ds take over point references
    _points.clear();
    _points.insert(_points.end(), frame_->points().begin(), frame_->points().end());
    for (FramePoint* frame_point: _points) {
      frame_point->setFrame(this);
    }

    //ds release points from old frame - without releasing the memory!
    frame_->points().clear();

    //ds spatial properties
    _robot_to_world              = frame_->robotToWorld();
    _world_to_robot              = frame_->worldToRobot();
    _robot_to_world_ground_truth = frame_->robotToWorldGroundTruth();

    //ds camera properties
    _camera_left           = frame_->cameraLeft();
    _camera_right          = frame_->cameraRight();
    _intensity_image_left  = frame_->intensityImageLeft();
    _intensity_image_right = frame_->intensityImageRight();
  }

  void Frame::setLocalMap(const LocalMap* local_map_) {
    _local_map          = local_map_;
    _frame_to_local_map = local_map_->worldToRobot()*this->robotToWorld();
    _local_map_to_frame = _frame_to_local_map.inverse();
  }

  const Count Frame::countPoints(const Count& min_track_length_,
                                 const ThreeValued& has_landmark_) const {
    Count count = 0;
    for (const FramePoint* frame_point: points()){
      if(frame_point->trackLength() < min_track_length_) {
        continue;
      }
      if(has_landmark_!= Unknown){
        if(has_landmark_ == True && !frame_point->landmark()) {
          continue;
        }
        if(has_landmark_ == False && frame_point->landmark()) {
          continue;
        }
      }
      ++count;
    }
    return count;
  }

  void Frame::setRobotToWorld(const TransformMatrix3D& robot_to_world_) {
    _robot_to_world = robot_to_world_;
    _world_to_robot = _robot_to_world.inverse();
  }

  //ds request new framepoint with optional link to a previous point (track)
  FramePoint* Frame::create(const cv::KeyPoint& keypoint_left_,
                            const cv::Mat& descriptor_left_,
                            const cv::KeyPoint& keypoint_right_,
                            const cv::Mat& descriptor_right_,
                            const PointCoordinates& camera_coordinates_left_,
                            FramePoint* previous_point_) {

    //ds allocate a new point connected to the previous one
    FramePoint* frame_point = new FramePoint(keypoint_left_,
                                             descriptor_left_,
                                             keypoint_right_,
                                             descriptor_right_,
                                             this);
    frame_point->setCameraCoordinatesLeft(camera_coordinates_left_);
    frame_point->setRobotCoordinates(cameraLeft()->cameraToRobot()*camera_coordinates_left_);

    //ds if the point is not linked (no track)
    if (previous_point_ == 0) {

      //ds this point has no predecessor
      frame_point->setOrigin(frame_point);
    } else {

      //ds connect the framepoints
      frame_point->setPrevious(previous_point_);
    }

    //ds update depth based on quality
    frame_point->setDepthMeters(camera_coordinates_left_.z());
    if (frame_point->depthMeters() < _maximum_depth_close) {
      frame_point->setIsNear(true);
    }
    return frame_point;
  }

  void Frame::releaseImages() {
    _intensity_image_left.release();
    _intensity_image_right.release();
  }

  void Frame::releasePoints() {
    for (const FramePoint* frame_point: _points) {
      delete frame_point;
    }
    _points.clear();
  }

  Frame* FramePointerMap::get(const Identifier& identifier_) {
    FramePointerMap::iterator it = find(identifier_);
    assert(it != end());
    return it->second;
  }

  void FramePointerMap::put(Frame* frame_) {
    assert(find(frame_->identifier()) == end());
    insert(std::make_pair(frame_->identifier(), frame_));
  }

  void FramePointerMap::replace(Frame* replacing_frame_) {
    FramePointerMap::iterator it = find(replacing_frame_->identifier());
    assert(it != end());
    Frame* frame_to_be_replaced = it->second;

    //ds update parent/child
    frame_to_be_replaced->previous()->setNext(replacing_frame_);
    replacing_frame_->setPrevious(frame_to_be_replaced->previous());
    if (frame_to_be_replaced->next() != 0) {
      replacing_frame_->setNext(frame_to_be_replaced->next());
    }

    //ds free old frame
    delete frame_to_be_replaced;

    //ds reinsert new frame porperly
    erase(replacing_frame_->identifier());
    put(replacing_frame_);
  }
}
