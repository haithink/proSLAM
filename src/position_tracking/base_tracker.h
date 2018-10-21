#pragma once
#include "framepoint_generation/base_framepoint_generator.h"
#include "aligners/base_frame_aligner.h"
#include "types/world_map.h"

namespace proslam {

//ds this class processes two subsequent Frames and establishes Framepoint correspondences (tracks) based on the corresponding images
class BaseTracker {

//ds exported types
public:

  //ds track proposal between 2 framepoints
  struct TrackCandidate {
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    TrackCandidate(FramePoint* framepoint_previous_,
                   const int32_t& row_,
                   const int32_t& col_,
                   const int32_t& pixel_distance_,
                   const real& matching_distance_): framepoint(framepoint_previous_),
                                                    row(row_),
                                                    col(col_),
                                                    pixel_distance(pixel_distance_),
                                                    descriptor_distance(matching_distance_) {}

    TrackCandidate(): framepoint(0),
                      row(0),
                      col(0),
                      pixel_distance(0),
                      descriptor_distance(0) {}

    FramePoint* framepoint;
    int32_t row;
    int32_t col;
    int32_t pixel_distance;
    real descriptor_distance;
  };
  typedef std::vector<TrackCandidate, Eigen::aligned_allocator<TrackCandidate>> TrackCandidateVector;
  typedef std::pair<const Identifier, TrackCandidateVector> TrackCandidateMapElement;
  typedef std::map<const Identifier, TrackCandidateVector, std::less<const Identifier>, Eigen::aligned_allocator<TrackCandidateMapElement>> TrackCandidateMap;

//ds object handling
PROSLAM_MAKE_PROCESSING_CLASS(BaseTracker)

//ds functionality
public:

  //! @brief creates a new Frame for the given images, retrieves the correspondences relative to the previous Frame, optimizes the current frame pose and updates landmarks
  virtual void compute();

  //! @breaks the track at the current frame
  //! @param[in] frame_ target frame to break the track at
  void breakTrack(Frame* frame_);

//ds getters/setters
public:

  const uint64_t& numberOfRecursiveRegistrations() const {return _number_of_recursive_registrations;}
  void setCameraLeft(const Camera* camera_left_) {_camera_left = camera_left_; _has_odometry = false;}
  void setOdometry(const TransformMatrix3D& odometry_) {_odometry = odometry_; _has_odometry = true;}
  void setAligner(BaseFrameAligner* pose_optimizer_) {_pose_optimizer = pose_optimizer_;}
  void setFramePointGenerator(BaseFramePointGenerator * framepoint_generator_) {_framepoint_generator = framepoint_generator_;}
  void setWorldMap(WorldMap* context_) {_context = context_;}
  void setIntensityImageLeft(const cv::Mat* intensity_image_left_) {_intensity_image_left = intensity_image_left_;}
  BaseFrameAligner* aligner() {return _pose_optimizer;}
  void setMotionPreviousToCurrent(const TransformMatrix3D& motion_previous_to_current_) {_previous_to_current_camera = motion_previous_to_current_;}
  BaseFramePointGenerator* framepointGenerator() {return _framepoint_generator;}
  const BaseFramePointGenerator* framepointGenerator() const {return _framepoint_generator;}
  const Count totalNumberOfTrackedPoints() const {return _total_number_of_tracked_points;}
  const Count totalNumberOfLandmarks() const {return _total_number_of_landmarks;}
  const Count binSize() const {return _parameters->bin_size_pixels;}
  const Count numberOfColsBin() const {return _number_of_cols_bin;}
  const Count numberOfRowsBin() const {return _number_of_rows_bin;}
  const real meanTrackingRatio() const {return _mean_tracking_ratio;}
  const real meanNumberOfKeypoints() const {return _mean_number_of_keypoints;}
  const real meanNumberOfFramepoints() const {return _mean_number_of_framepoints;}

//ds helpers
protected:

  //ds retrieves framepoint correspondences between previous and current frame
  void _track(const Frame* previous_frame_,
              Frame* current_frame_,
              const TransformMatrix3D& previous_to_current_,
              const bool& track_by_appearance_ = false);

  //! @brief recursive registration method, that calls track framepoints with different parameters upon failure
  //! @param [in] frame_previous_ the previous frame
  //! @param [in] frame_current_ the current frame to align against the previous frame
  void _registerRecursive(Frame* frame_previous_,
                          Frame* frame_current_,
                          TransformMatrix3D previous_to_current_,
                          const Count& recursion_ = 0);

  //ds adds a new track
  void _addTrack(TrackCandidate& track_candidate_, Frame* current_frame_);

  //! @brief clears framepoint matrix based on active tracks
  void _clearFramepoints();

  //ds adds new framepoints to the provided frame (picked from the pool of the _framepoint_generator)
  void _addPoints(Frame* frame_);

  //ds retrieves framepoint projections as image coordinates in a vector (at the same time removing points with invalid projections)
  void _project(ImageCoordinatesVector& predictions_left_,
                FramePointPointerVector& previous_points_visible_,
                const Frame* previous_frame_,
                const Frame* current_frame_,
                const TransformMatrix3D& previous_to_current_) const;

  //ds prunes invalid tracks after pose optimization
  void _prunePoints(Frame* frame_);

  //ds updates existing or creates new landmarks for framepoints of the provided frame
  void _updatePoints(WorldMap* context_, Frame* frame_);

  //ds attempts to recover framepoints in the current image using the more precise pose estimate, retrieved after pose optimization
  virtual void _recoverPoints(Frame* current_frame_) = 0;

  //ds creates a frame, which is filled by calling the framepoint generator
  virtual Frame* _createFrame() = 0;

//ds attributes
protected:

  //ds tracker state
  Frame::Status _status                        = Frame::Localizing;
  Frame::Status _status_previous               = Frame::Localizing;
  uint64_t _number_of_recursive_registrations  = 0;

  //ds running variables and buffered values
  Count _number_of_tracked_landmarks          = 0;
  Count _number_of_potential_points           = 0;
  Count _number_of_tracked_points             = 0;
  Count _number_of_tracked_landmarks_previous = 0;
  real _tracking_ratio                        = 0;
  real _mean_tracking_ratio                   = 0;
  Count _number_of_tracked_frames             = 0;
  const Camera* _camera_left;

  //! @brief currently active projection tracking distance (adjusted dynamically at runtime)
  int32_t _projection_tracking_distance_pixels;

  //gg working elements
  const cv::Mat* _intensity_image_left;
  WorldMap* _context;

  //gg processing objects
  BaseFrameAligner* _pose_optimizer;
  BaseFramePointGenerator* _framepoint_generator;

  //! @brief position tracking bookkeeping
  TransformMatrix3D _previous_to_current_camera = TransformMatrix3D::Identity();

  //ds framepoint track recovery
  Count _number_of_lost_points      = 0;
  Count _number_of_recovered_points = 0;
  FramePointPointerVector _lost_points;

  //ds feature density regularization
  Count _number_of_rows_bin;
  Count _number_of_cols_bin;
  FramePointMatrix _bin_map_left;
  bool _enable_keypoint_binning = false;

  //ds stats only
  real _mean_number_of_keypoints   = 0;
  real _mean_number_of_framepoints = 0;

private:

  //ds informative only
  CREATE_CHRONOMETER(tracking)
  CREATE_CHRONOMETER(track_creation)
  CREATE_CHRONOMETER(pose_optimization)
  CREATE_CHRONOMETER(landmark_optimization)
  CREATE_CHRONOMETER(point_recovery)
  Count _total_number_of_tracked_points = 0;
  Count _total_number_of_landmarks      = 0;
  bool _has_odometry;
  TransformMatrix3D _odometry;
  TransformMatrix3D _previous_odometry;
};
}
