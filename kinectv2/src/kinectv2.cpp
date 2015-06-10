/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2011 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */


#include <iostream>
#include <signal.h>

#include <opencv2/opencv.hpp>

#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/threading.h>

#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>

void thread_main( libfreenect2::Freenect2Device *dev );

bool sflag = false;
bool enableRGB = false;
bool enableIR = false;

int main(int argc, char** argv){

	ros::init(argc, argv, "kinectv2");

	ros::NodeHandle local_nh("~");
	if (!local_nh.hasParam("enable_rgb")){
		local_nh.setParam("enable_rgb", false);
	}
	if (!local_nh.hasParam("enable_ir")){
		local_nh.setParam("enable_ir", false);
	}

	if (!local_nh.getParam("enable_rgb", enableRGB)){
		ROS_ERROR("parameter enable_rgb is invalid.");
		return -1;
	}

	if (!local_nh.getParam("enable_ir", enableIR)){
		ROS_ERROR("parameter enable_ir is invalid.");
		return -1;
	}

	libfreenect2::Freenect2 freenect2;
	libfreenect2::Freenect2Device *dev = freenect2.openDefaultDevice();

	if(dev == 0)
	{
		std::cout << "no device connected or failure opening the default one!" << std::endl;
		return -1;
	}

	pthread_t thread;
	pthread_create( &thread, NULL, (void* (*)(void*))thread_main, dev );

	ros::spin();

	sflag = true;
	pthread_join( thread, NULL );

	dev->stop();
	dev->close();

	return 0;
}

void thread_main( libfreenect2::Freenect2Device *dev ){
	ROS_INFO("New thread Created.");

	unsigned int listener_flag = libfreenect2::Frame::Depth;

	if( enableRGB )	listener_flag |= libfreenect2::Frame::Color;
	if( enableIR )	listener_flag |= libfreenect2::Frame::Ir;

	libfreenect2::SyncMultiFrameListener listener(listener_flag);
	libfreenect2::FrameMap frames;

	if( enableRGB ){
		dev->setColorFrameListener(&listener);
	}
	dev->setIrAndDepthFrameListener(&listener);
	dev->start();

	std::cout << "device serial: " << dev->getSerialNumber() << std::endl;
	std::cout << "device firmware: " << dev->getFirmwareVersion() << std::endl;

	ros::NodeHandle n;
	image_transport::ImageTransport it(n);

	image_transport::Publisher rgb_pub;
	image_transport::Publisher depth_pub;
	image_transport::Publisher ir_pub;

	if( enableRGB )	rgb_pub = it.advertise("kinect/rgb", 1);
	depth_pub = it.advertise("kinect/depth", 1);
	if( enableIR )	ir_pub = it.advertise("kinect/ir", 1);

	while(!sflag)
	{
		std_msgs::Header head = std_msgs::Header();
		listener.waitForNewFrame(frames);
		head.stamp = ros::Time::now();

		libfreenect2::Frame *rgb = frames[libfreenect2::Frame::Color];
		libfreenect2::Frame *ir = frames[libfreenect2::Frame::Ir];
		libfreenect2::Frame *depth = frames[libfreenect2::Frame::Depth];

		//cv::imshow("rgb", cv::Mat(rgb->height, rgb->width, CV_8UC3, rgb->data));
		//cv::imshow("ir", cv::Mat(ir->height, ir->width, CV_32FC1, ir->data) / 20000.0f);
		//cv::imshow("depth", cv::Mat(depth->height, depth->width, CV_32FC1, depth->data) / 4500.0f);

		if( enableRGB ){
			cv::Mat rgbMat = cv::Mat(rgb->height, rgb->width, CV_8UC3, rgb->data);
			cv::cvtColor( rgbMat, rgbMat, CV_BGR2RGB);
			sensor_msgs::ImagePtr rgb_msg = cv_bridge::CvImage(head, "rgb8", rgbMat ).toImageMsg();
			rgb_pub.publish(rgb_msg);
		}

		cv::Mat depthMat = cv::Mat(depth->height, depth->width, CV_32FC1, depth->data);
		depthMat.convertTo(depthMat, CV_16UC1, 1.0f);
		sensor_msgs::ImagePtr depth_msg = cv_bridge::CvImage(head, "mono16", depthMat ).toImageMsg();
		depth_pub.publish(depth_msg);

		if( enableIR ){
			cv::Mat irMat = cv::Mat(ir->height, ir->width, CV_32FC1, ir->data) / 20000.0f;
			irMat.convertTo(irMat, CV_8UC1, 255.0f);
			sensor_msgs::ImagePtr ir_msg = cv_bridge::CvImage(head, "mono8", irMat ).toImageMsg();
			ir_pub.publish(ir_msg);
		}

		listener.release(frames);

		//libfreenect2::this_thread::sleep_for(libfreenect2::chrono::milliseconds(1));
	}
}
