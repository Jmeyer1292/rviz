/*
 * Copyright (c) 2009, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "arrow_marker.h"
#include "marker_selection_handler.h"
#include "rviz/default_plugin/marker_display.h"

#include "rviz/visualization_manager.h"
#include "rviz/selection/selection_manager.h"

#include <ogre_tools/arrow.h>

#include <tf/transform_listener.h>

#include <OGRE/OgreVector3.h>
#include <OGRE/OgreQuaternion.h>
#include <OGRE/OgreSceneNode.h>

namespace rviz
{

ArrowMarker::ArrowMarker(MarkerDisplay* owner, VisualizationManager* manager, Ogre::SceneNode* parent_node)
: MarkerBase(owner, manager, parent_node)
, arrow_(0)
{
}

ArrowMarker::~ArrowMarker()
{
  delete arrow_;
}

void ArrowMarker::onNewMessage(const MarkerConstPtr& old_message, const MarkerConstPtr& new_message)
{
  ROS_ASSERT(new_message->type == visualization_msgs::Marker::ARROW);

  if (!new_message->points.empty() && new_message->points.size() < 2)
  {
    std::stringstream ss;
    ss << "Arrow marker [" << getStringID() << "] only specified one point of a point to point arrow.";
    if ( owner_ )
    {
      owner_->setMarkerStatus(getID(), status_levels::Error, ss.str());
    }
    ROS_DEBUG("%s", ss.str().c_str());

    delete arrow_;
    arrow_ = 0;

    return;
  }

  if (!arrow_)
  {
    arrow_ = new ogre_tools::Arrow(vis_manager_->getSceneManager(), scene_node_);
    coll_ = vis_manager_->getSelectionManager()->createCollisionForObject(arrow_, SelectionHandlerPtr(new MarkerSelectionHandler(this, MarkerID(new_message->ns, new_message->id))), coll_);
  }

  Ogre::Vector3 pos, scale;
  Ogre::Quaternion orient;
  transform(new_message, pos, orient, scale);

  if (new_message->points.empty())
  {
    if ( owner_ && (new_message->scale.x * new_message->scale.y * new_message->scale.z == 0.0f) )
    {
      owner_->setMarkerStatus(getID(), status_levels::Warn, "Scale of 0 in one of x/y/z");
    }

    //we need base_orient, since the arrow goes along the -z axis by default (for historical reasons)
    Ogre::Quaternion orient_x = Ogre::Quaternion( Ogre::Radian(-Ogre::Math::HALF_PI), Ogre::Vector3::UNIT_Y );
    
    scene_node_->setPosition(pos);
    scene_node_->setOrientation( orient * orient_x );
    arrow_->setScale(scale);
  }
  else
  {
    const geometry_msgs::Point& start_pos = new_message->pose.position;
    const geometry_msgs::Point& p1 = new_message->points[0];
    const geometry_msgs::Point& p2 = new_message->points[1];

    tf::Stamped<tf::Point> t_p1;
    tf::Stamped<tf::Point> t_p2;
    try
    {
      vis_manager_->getTFClient()->transformPoint(vis_manager_->getFixedFrame(), tf::Stamped<tf::Point>(tf::Point(p1.x, p1.y, p1.z) + tf::Point(start_pos.x, start_pos.y, start_pos.z), new_message->header.stamp, new_message->header.frame_id), t_p1);
      vis_manager_->getTFClient()->transformPoint(vis_manager_->getFixedFrame(), tf::Stamped<tf::Point>(tf::Point(p2.x, p2.y, p2.z) + tf::Point(start_pos.x, start_pos.y, start_pos.z), new_message->header.stamp, new_message->header.frame_id), t_p2);
    }
    catch(tf::TransformException& e)
    {
      ROS_DEBUG( "Error transforming marker [%s/%d] from frame [%s] to frame [%s]: %s\n", new_message->ns.c_str(), new_message->id, new_message->header.frame_id.c_str(), vis_manager_->getFixedFrame().c_str(), e.what() );
      delete arrow_;
      arrow_ = 0;
      return;
    }

    Ogre::Vector3 point1(t_p1.x(), t_p1.y(), t_p1.z());
    Ogre::Vector3 point2(t_p2.x(), t_p2.y(), t_p2.z());

    Ogre::Vector3 direction = point2 - point1;
    float distance = direction.length();
    direction.normalise();
    Ogre::Quaternion orient = Ogre::Vector3::NEGATIVE_UNIT_Z.getRotationTo( direction );
    scene_node_->setPosition(point1);
    scene_node_->setOrientation(orient);
    arrow_->setScale(Ogre::Vector3(1.0f, 1.0f, 1.0f));

    float head_length = 0.1*distance;
    if ( new_message->scale.z != 0.0 )
    {
      head_length = new_message->scale.z;
    }
    float shaft_length = distance - head_length;
    arrow_->set(shaft_length, new_message->scale.x, head_length, new_message->scale.y);
  }

  arrow_->setColor(new_message->color.r, new_message->color.g, new_message->color.b, new_message->color.a);
}

}