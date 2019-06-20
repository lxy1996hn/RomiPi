#!/usr/bin/env python
#
# romipi_fiducials.py
#
# The purpose of this program is to recognize aruco fiducials
# for localization
# 
# Peter F. Klemperer
# June 20, 2019
#
# Design Plan
# 1. Basic Publisher / Subscriber
# 2. Subscribe to images from pi
# 3. Publish images for debug
# 4. Open the Aruco Library
# 5. Apply Aruco and publish the fiducials 
#
# Look at the C++ based example in aruco_detect.py
# from the aruco_detect library located at:
# 

import rospy
from romipi_astar.romipi_driver import AStar

# message types
from std_msgs.msg import String
from sensor_msgs.msg import CompressedImage

# opencv2 realted
import cv2
import numpy as np
import math
import cv2.aruco as aruco

VERBOSE = False

# FIXME make this a proper ROS parameter
camera_param_file = "/home/mhc/multi-robotics/robots/Romi-All/Romi/camera/cameraParameters-PK-RasberryPi-Camera-8MP.xml"

class FiducialsNode():
    def __init__(self):
        ''' initialize subscriber from camera and publish image, fiducials '''

        rospy.init_node('romipi_fiducials_node', anonymous=True)

        self.image_publish = rospy.Publisher("/romipi_fiducials_node/fiducial_image/compressed", CompressedImage, queue_size=1)

        if VERBOSE:
            print("subscribed to /camera/image/compressed")

        self.compressed_camera_image = None

        # configure cv2 aruco handling
        self.aruco_dict = aruco.Dictionary_get(aruco.DICT_6X6_250)
        camera_reader = cv2.FileStorage()
        camera_reader.open(camera_param_file, cv2.FileStorage_READ)
        # camera configurations
        self.camera_matrix = self.read_node_matrix(camera_reader, "cameraMatrix")
        self.dist_coeffs   = self.read_node_matrix(camera_reader, "dist_coeffs")
        width, height = self.read_resolution(camera_reader) # FIXME seems unused
        if self.camera_matrix is None or self.dist_coeffs is None:
            print("camera configurations not loaded.")

        if VERBOSE :
            print("camera matrix : %s" % self.camera_matrix)
            print("distance coeffs : %s" % self.dist_coeffs)
            print("width, height : %d, %d" % (width,height))

    # FIXME may not be used
    @staticmethod
    def read_resolution(reader):
        node = reader.getNode("cameraResolution")
        return int(node.at(0).real()), int(node.at(1).real())
 
    @staticmethod
    def read_node_matrix(reader, name):
        node = reader.getNode(name)
        return node.mat()

    def camera_callback(self, ros_data):
        if VERBOSE:
            print 'received image of type: "%s"' % ros_data.format
        self.compressed_camera_image = ros_data

    def process_camera_image(self, frame):
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

        corners, ids, rejectedImgPoints = aruco.detectMarkers(gray, self.aruco_dict)
        frame = aruco.drawDetectedMarkers(frame, corners)
        return frame

    def romipi_fiducials_node(self):
        camera_ros_path = "/raspicam_node/image/compressed"
        self.camera_subscriber = rospy.Subscriber(camera_ros_path, CompressedImage, self.camera_callback, queue_size = 1)
        
        rate = rospy.Rate(1) # rate is hz
        while not rospy.is_shutdown():
            # read image saved by the camera_callback
            if self.compressed_camera_image is not None:
                # ROS image to cv2 format
                np_arr = np.fromstring(self.compressed_camera_image.data, np.uint8)
                frame = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)
                fiducial_frame_np = self.process_camera_image(frame)

                # convert processed frame to ROS format
                msg = CompressedImage()
                msg.header.stamp = rospy.Time.now()
                msg.format = "jpeg"
                msg.data = np.array(cv2.imencode('.jpg', fiducial_frame_np)[1]).tostring()
                self.compressed_fiducial_image = msg
                # publish debug image
                self.image_publish.publish(self.compressed_fiducial_image)
                # publish fiducial locations
            rate.sleep() 
        
if __name__ == '__main__':
    try:
        fiducials = FiducialsNode()
        fiducials.romipi_fiducials_node()
    except rospy.ROSInterruptException:
        pass

