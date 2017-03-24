#include "graph_optimizer.h"

namespace proslam {

  GraphOptimizer::GraphOptimizer(): _optimizer(getOptimizer()) {
    std::cerr << "GraphOptimizer::GraphOptimizer|constructed" << std::endl;
  }

  GraphOptimizer::~GraphOptimizer(){
    std::cerr << "GraphOptimizer::GraphOptimizer|destroying" << std::endl;
    std::cerr << "GraphOptimizer::GraphOptimizer|destroyed" << std::endl;
  }

  void GraphOptimizer::optimize(WorldMap* context_) {
    CHRONOMETER_START(overall)
    optimizePoses(context_);
    optimizeLandmarks(context_);
    CHRONOMETER_STOP(overall)
  }

  //ds optimizes all landmarks by computing rudely the average without using g2o
  void GraphOptimizer::optimizeLandmarks(WorldMap* context_){

    //ds update all active landmark positions
    for (const LocalMap* local_map: context_->localMaps()) {
      for (Landmark::Item* item: local_map->items()) {

        //ds buffer current landmark
        Landmark* landmark = item->landmark;
        assert(landmark);

        //ds update landmark position
        landmark->resetCoordinates(local_map->robotToWorld()*item->robot_coordinates);
        landmark->setIsOptimized(true);
        landmark->setIsClosed(true);
      }
    }
  }

  void GraphOptimizer::optimizePoses(WorldMap* context_) {
    _optimizer->clear();
    _optimizer->clearParameters();

    //ds vertex linking
    g2o::VertexSE3* vertex_keyframe_previous = 0;
    TransformMatrix3D world_to_frame_previous(TransformMatrix3D::Identity());

    //ds added measurements
    LocalMapPointerVector keyframes_in_graph(0);
    
    //ds pose measurements
    for (LocalMap* keyframe: context_->localMaps()) {

      //ds add the pose to g2o
      g2o::VertexSE3* vertex_keyframe = new g2o::VertexSE3;
      vertex_keyframe->setId(keyframe->index());
      vertex_keyframe->setEstimate(keyframe->robotToWorld().cast<double>());
      _optimizer->addVertex(vertex_keyframe);

      //ds if previous pose is not set (first frame)
      if (0 == vertex_keyframe_previous) {
        vertex_keyframe->setFixed(true);
      } else {
        _optimizer->addEdge(getPoseEdge(_optimizer,
                                        vertex_keyframe->id(),
                                        vertex_keyframe_previous->id(),
                                        world_to_frame_previous*keyframe->robotToWorld()));
      }

      //ds update previous
      vertex_keyframe_previous = vertex_keyframe;
      world_to_frame_previous = keyframe->worldToRobot();
      keyframes_in_graph.push_back(keyframe);
    }

    //ds pose measurements: check for closures now as all id's are in the graph
    for (const LocalMap* keyframe_query: context_->localMaps()) {

      for (const LocalMap::LocalMapCorrespondence& keyframe_correspondence: keyframe_query->closures()) {
        const LocalMap* keyframe_reference = keyframe_correspondence.keyframe;
        const TransformMatrix3D transform_query_to_reference = keyframe_correspondence.relation.transform;

        //ds compute required transform delta
        const TransformMatrix3D transform_query_to_reference_current = keyframe_reference->worldToRobot()*keyframe_query->robotToWorld();
        const Vector3 translation_delta = transform_query_to_reference.translation()-transform_query_to_reference_current.translation();

        //ds check threshold
        if (4.0 < translation_delta.norm()) {
          std::cerr << "Mapper::optimizePoses|WARNING: adding large impact closure: " << keyframe_query->index() << " > " << keyframe_reference->index() 
                    << " translation L1: " << translation_delta.norm() << std::endl;
        }

        //ds retrieve closure edge
        g2o::EdgeSE3* edge_closure = getClosureEdge(_optimizer,
                                                    keyframe_query->index(),
                                                    keyframe_reference->index(),
                                                    transform_query_to_reference);
        _optimizer->addEdge(edge_closure);
      }
    }

    //ds optimize poses
    _optimizer->initializeOptimization();
    _optimizer->setVerbose(false);
    _optimizer->optimize(10);

    //ds backpropagate solution to tracking context: keyframes
    for (LocalMap* keyframe: keyframes_in_graph) {
      g2o::VertexSE3* vertex_keyframe = dynamic_cast<g2o::VertexSE3*>(_optimizer->vertex(keyframe->index()));
      assert(0 != vertex_keyframe);
      keyframe->setRobotToWorld(vertex_keyframe->estimate().cast<real>());
    }
    context_->setRobotToWorldPrevious(context_->currentLocalMap()->robotToWorld());
  }
}
