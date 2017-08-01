//==============================================================================
// Copyright (c) 2012, Johannes Meyer, TU Darmstadt
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the Flight Systems and Automatic Control group,
//       TU Darmstadt, nor the names of its contributors may be used to
//       endorse or promote products derived from this software without
//       specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//==============================================================================

#ifndef UBLOX_GPS_NODE_H
#define UBLOX_GPS_NODE_H

// STL
#include <vector>
#include <set>
// Boost
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
// ROS includes
#include <ros/ros.h>
#include <ros/console.h>
#include <ros/serialization.h>
#include <diagnostic_updater/diagnostic_updater.h>
#include <diagnostic_updater/publisher.h>
// ROS messages
#include <geometry_msgs/TwistWithCovarianceStamped.h>
#include <geometry_msgs/Vector3Stamped.h>
#include <sensor_msgs/NavSatFix.h>
// Other U-Blox package includes
#include <ublox_msgs/ublox_msgs.h>
// Ublox GPS includes
#include <ublox_gps/gps.h>
#include <ublox_gps/utils.h>

// This file declares the ComponentInterface which acts as a high level 
// interface for u-blox firmware, product categories, etc. It contains methods
// to configure the u-blox and subscribe to u-blox messages.
//
// This file also declares UbloxNode which implements ComponentInterface and is 
// the main class and ros node. it implements functionality which applies to
// any u-blox device, regardless of the firmware version or product type. 
// The class is designed in compositional style; it contains ComponentInterfaces 
// which implement features specific to the device based on its firmware version
// and product category. UbloxNode calls the public methods of each component.
//
// This file declares UbloxFirmware is an abstract class which implements
// ComponentInterface and functions generic to all firmware (such as the 
// initializing the fix diagnostics). Subclasses of UbloxFirmware for firmware 
// versions 6-8 are also declared in this file.
//
// Lastly, this file declares classes for each product category which also 
// implement u-blox interface, currently only the class for High Precision 
// GNSS devices has been fully implemented and tested.

namespace ublox_node {

//! Queue size for ROS publishers
const static uint32_t kROSQueueSize = 1; 
//! Default measurement period for HPG devices
const static uint16_t kDefaultMeasPeriod = 250; 
//! Default subscribe Rate to u-blox messages [Hz]
const static uint32_t kSubscribeRate = 1; 
//! Subscribe Rate for u-blox SV Info messages
const static uint32_t kNavSvInfoSubscribeRate = 20; 

// ROS objects
//! ROS diagnostic updater
boost::shared_ptr<diagnostic_updater::Updater> updater; 
//! fix frequency diagnostic updater
boost::shared_ptr<diagnostic_updater::TopicDiagnostic> freq_diag; 
//! Node Handle for GPS node
boost::shared_ptr<ros::NodeHandle> nh; 

//! Handles communication with the U-Blox Device
ublox_gps::Gps gps; 
//! Which GNSS are supported by the device
std::set<std::string> supported; 
//! Whether or not to enable the given message subscriber
std::map<std::string, bool> enabled; 
//! The ROS frame ID of this GPS
std::string frame_id; 
//! The fix status service type, set based on the enabled GNSS
int fix_status_service; 
//! The measurement [ms], see CfgRate.msg
uint16_t meas_rate; 
//! Navigation rate in measurement cycles, see CfgRate.msg
uint16_t nav_rate; 
//! IDs of RTCM out messages to configure
std::vector<uint8_t> rtcm_ids; 
//! Rates of RTCM out messages. Size must be the same as rtcm_ids
std::vector<uint8_t> rtcm_rates; 

/**
 * @brief Determine dynamic model from human-readable string.
 * @param model One of the following (case-insensitive):
 *  - portable
 *  - stationary
 *  - pedestrian
 *  - automotive
 *  - sea
 *  - airborne1
 *  - airborne2
 *  - airborne4
 *  - wristwatch
 * @return DynamicModel
 * @throws std::runtime_error on invalid argument.
 */
uint8_t modelFromString(const std::string& model);

/**
 * @brief Determine fix mode from human-readable string.
 * @param mode One of the following (case-insensitive):
 *  - 2d
 *  - 3d
 *  - auto
 * @return FixMode
 * @throws std::runtime_error on invalid argument.
 */
uint8_t fixModeFromString(const std::string& mode);

/**
 * @brief Check that the parameter is above the minimum.
 * @param val the value to check
 * @param min the minimum for this value
 * @param name the name of the parameter
 * @throws std::runtime_error if it is below the minimum
 */
template <typename V, typename T>
void checkMin(V val, T min, std::string name) {
  if(val < min) {
    std::stringstream oss;
    oss << "Invalid settings: " << name << " must be > " << min;
    throw std::runtime_error(oss.str());
  }
}

/**
 * @brief Check that the parameter is in the range.
 * @param val the value to check
 * @param min the minimum for this value
 * @param max the maximum for this value
 * @param name the name of the parameter
 * @throws std::runtime_error if it is out of bounds
 */
template <typename V, typename T>
void checkRange(V val, T min, T max, std::string name) {
  if(val < min || val > max) {
    std::stringstream oss;
    oss << "Invalid settings: " << name << " must be in range [" << min << 
        ", " << max << "].";
    throw std::runtime_error(oss.str());
  }
}

/**
 * @brief Check that the elements of the vector are in the range.
 * @param val the vector to check
 * @param min the minimum for this value
 * @param max the maximum for this value
 * @param name the name of the parameter
 * @throws std::runtime_error value it is out of bounds
 */
template <typename V, typename T>
bool checkRange(std::vector<V> val, T min, T max, std::string name) {
  for(size_t i = 0; i < val.size(); i++)  {
    std::stringstream oss;
    oss << name << "[" << i << "]";
    checkRange(val[i], min, max, name);
  }
}

/**
 * @brief Get a unsigned integer value from the parameter server.
 * @param key the key to be used in the parameter server's dictionary
 * @param u storage for the retrieved value.
 * @throws std::runtime_error if the parameter is out of bounds
 * @return true if found, false if not found.
 */
template <typename U>
bool getRosUint(const std::string& key, U &u) {
  int param;
  if (!nh->getParam(key, param)) return false;
  // Check the bounds
  U min = 0;
  U max = ~0;
  checkRange(param, min, max, key);
  // set the output
  u = (U) param;
  return true;
}

/**
 * @brief Get a unsigned integer value from the parameter server.
 * @param key the key to be used in the parameter server's dictionary
 * @param u storage for the retrieved value.
 * @param val value to use if the server doesn't contain this parameter.
 * @throws std::runtime_error if the parameter is out of bounds
 * @return true if found, false if not found.
 */
template <typename U, typename V>
void getRosUint(const std::string& key, U &u, V default_val) {
  if(!getRosUint(key, u))
    u = default_val;
}

/**
 * @brief Get a unsigned integer vector from the parameter server.
 * @throws std::runtime_error if the parameter is out of bounds.
 * @return true if found, false if not found.
 */
template <typename U>
bool getRosUint(const std::string& key, std::vector<U> &u) {
  std::vector<int> param;
  if (!nh->getParam(key, param)) return false;
  
  // Check the bounds
  U min = 0;
  U max = ~0;
  checkRange(param, min, max, key);

  // set the output
  u.insert(u.begin(), param.begin(), param.end());
  return true;
}

/**
 * @brief Get a integer (size 8 or 16) value from the parameter server.
 * @param key the key to be used in the parameter server's dictionary
 * @param u storage for the retrieved value.
 * @throws std::runtime_error if the parameter is out of bounds
 * @return true if found, false if not found.
 */
template <typename I>
bool getRosInt(const std::string& key, I &u) {
  int param;
  if (!nh->getParam(key, param)) return false;
  // Check the bounds
  I min = 1 << (sizeof(I) * 8 - 1);
  I max = (I)~(min);
  checkRange(param, min, max, key);
  // set the output
  u = (I) param;
  return true;
}

/**
 * @brief Get an integer value (size 8 or 16) from the parameter server.
 * @param key the key to be used in the parameter server's dictionary
 * @param u storage for the retrieved value.
 * @param val value to use if the server doesn't contain this parameter.
 * @throws std::runtime_error if the parameter is out of bounds
 * @return true if found, false if not found.
 */
template <typename U, typename V>
void getRosInt(const std::string& key, U &u, V default_val) {
  if(!getRosInt(key, u))
    u = default_val;
}

/**
 * @brief Get a int (size 8 or 16) vector from the parameter server.
 * @throws std::runtime_error if the parameter is out of bounds.
 * @return true if found, false if not found.
 */
template <typename I>
bool getRosInt(const std::string& key, std::vector<I> &i) {
  std::vector<int> param;
  if (!nh->getParam(key, param)) return false;
  
  // Check the bounds
  I min = 1 << (sizeof(I) * 8 - 1);
  I max = (I)~(min);
  checkRange(param, min, max, key);

  // set the output
  i.insert(i.begin(), param.begin(), param.end());
  return true;
}

/**
 * @brief Publish a ROS message of type MessageT. 
 *
 * @details This function should be used to publish all messages which are 
 * simply read from u-blox and published.
 * @param m the message to publish
 * @param topic the topic to publish the message on
 */
template <typename MessageT>
void publish(const MessageT& m, const std::string& topic) {
  static ros::Publisher publisher =
      nh->advertise<MessageT>(topic, kROSQueueSize);
  publisher.publish(m);
}

/**
 * @param gnss The string representing the GNSS. Refer MonVER message protocol.
 * i.e. GPS, GLO, GAL, BDS, QZSS, SBAS, IMES
 * @return true if the device supports the given GNSS
 */
bool supportsGnss(std::string gnss) {
  return supported.count(gnss) > 0;
}

/** 
 * @brief This interface is used to add functionality to the main node. 
 *
 * @details This interface is generic and can be implemented for other features 
 * besides the main node, hardware versions, and firmware versions.
 */
class ComponentInterface {
 public:
  /**
   * @brief Get the ROS parameters.
   * @throws std::runtime_error if a parameter is invalid or required
   * parameters are not set.
   */
  virtual void getRosParams() = 0;
  
  /**
   * @brief Configure the U-Blox settings.
   * @return true if configured correctly, false otherwise
   */
  virtual bool configureUblox() = 0;

  /**
   * @brief Initialize the diagnostics. 
   *
   * @details Function may be empty.
   */
  virtual void initializeRosDiagnostics() = 0;

  /**
   * @brief Subscribe to u-blox messages and publish as ROS messages.
   */
  virtual void subscribe() = 0;
};

typedef boost::shared_ptr<ComponentInterface> ComponentPtr;

/**
 * @brief This class represents u-blox ROS node for *all* firmware and product 
 * versions.
 * 
 * @details It loads the user parameters, configures the u-blox 
 * device, subscribes to u-blox messages, and configures the device hardware. 
 * Functionality specific to a given product or firmware version, etc. should 
 * NOT be implemented in this class. Instead, the user should add the 
 * functionality to the appropriate implementation of ComponentInterface.
 * If necessary, the user should create a class which implements u-blox 
 * interface, then add a pointer to an instance of the class to the 
 * components vector.
 * The UbloxNode calls the public methods of ComponentInterface for each  
 * element in the components vector.
 */
class UbloxNode : public virtual ComponentInterface {
 public:
  //! how often (in seconds) to call poll messages
  const static double kPollDuration = 1.0; 
  // Constants used for diagnostic frequency updater
  //! [s] 5Hz diagnostic period
  const static float kDiagnosticPeriod = 0.2; 
  //! Tolerance for Fix topic frequency as percentage of target frequency
  const static double kFixFreqTol = 0.15; 
  //! Window [num messages] for Fix Frequency Diagnostic
  const static double kFixFreqWindow = 10; 
  //! Minimum Time Stamp Status for fix frequency diagnostic
  const static double kTimeStampStatusMin = 0; 

  /**
   * @brief Initialize and run the u-blox node.
   */
  UbloxNode();

  /**
   * @brief Get the node parameters from the ROS Parameter Server.
   */
  void getRosParams();

  /**
   * @brief Configure the device based on ROS parameters.
   * @return true if configured successfully
   */
  bool configureUblox();

  /**
   * @brief Subscribe to all requested u-blox messages.
   */
  void subscribe();
  
  /**
   * @brief Initialize the diagnostic updater and add the fix diagnostic.
   */
  void initializeRosDiagnostics();

  /**
   * @brief Print an INF message to the ROS console.
   */
  void printInf(const ublox_msgs::Inf &m, uint8_t id);

 private:

  /**
   * @brief Initialize the I/O handling.
   */
  void initializeIo();
  
  /**
   * @brief Initialize the U-Blox node. Configure the U-Blox and subscribe to 
   * messages.
   */
  void initialize();

  /**
   * @brief Shutdown the node. Closes the serial port.
   */
  void shutdown();

  /**
   * @brief Send a reset message the u-blox device & re-initialize the I/O.
   * @return true if reset was successful, false otherwise.
   */
  bool resetDevice();

  /**
   * @brief Process the MonVer message and add firmware and product components.
   *
   * @details Determines the protocol version, product type and supported GNSS.
   */
  void processMonVer();

  /**
   * @brief Add the interface for firmware specific configuration, subscribers, 
   * & diagnostics. This assumes the protocol_version_ has been set.
   */
  void addFirmwareInterface();

  /**
   * @brief Add the interface which is used for product category 
   * configuration, subscribers, & diagnostics.
   * @param the product category, i.e. SPG, HPG, ADR, UDR, TIM, or FTS.
   * @param for HPG/TIM products, this value is either REF or ROV, for other
   * products this string is empty
   */
  void addProductInterface(std::string product_category, 
                           std::string ref_rov = "");

  /**
   * @brief Poll messages from the U-Blox device.
   * @param event a timer indicating how often to poll the messages
   */
  void pollMessages(const ros::TimerEvent& event);

  /**
   * @brief Configure INF messages, call after subscribe.
   */
  void configureInf();
  
  //! The u-blox node components
  /*! 
   * The node will call the functions in these interfaces for each object
   * in the vector.
   */
  std::vector<boost::shared_ptr<ComponentInterface> > components_;

  //! Used for diagnostic updater, set from constants
  double min_freq;
  //! Used for diagnostic updater, set from constants
  double max_freq; 

  //! Determined From Mon VER
  float protocol_version_; 
  // Variables set from parameter server
  //! Device port
  std::string device_; 
  //! dynamic model type
  std::string dynamic_model_; 
  //! Fix mode type
  std::string fix_mode_; 
  //! Set from dynamic model string
  uint8_t dmodel_; 
  //! Set from fix mode string
  uint8_t fmode_; 
  //! UART1 baudrate
  uint32_t baudrate_; 
  //! UART in protocol (see CfgPRT message for constants)
  uint16_t uart_in_; 
  //! UART out protocol (see CfgPRT message for constants)
  uint16_t uart_out_;
  //! USB TX Ready Pin configuration (see CfgPRT message for constants)
  uint16_t usb_tx_;
  //! Whether to configure the USB port 
  /*! Set to true if usb_in & usb_out parameters are set */
  bool set_usb_;
  //! USB in protocol (see CfgPRT message for constants)
  uint16_t usb_in_;
  //! USB out protocol (see CfgPRT message for constants)
  uint16_t usb_out_ ;
  //! The measurement rate in Hz
  double rate_; 
  //! If true, set configure the User-Defined Datum
  bool set_dat_; 
  //! User-defined Datum
  ublox_msgs::CfgDAT cfg_dat_; 
  //! Whether or not to enable SBAS
  bool enable_sbas_; 
  //! Whether or not to enable PPP (advanced setting)
  bool enable_ppp_; 
  //! SBAS Usage parameter (see CfgSBAS message) 
  uint8_t sbas_usage_;
  //! Max SBAS parameter (see CfgSBAS message) 
  uint8_t max_sbas_;
  //! Dead reckoning limit parameter
  uint8_t dr_limit_;
};

/** 
 * @brief This abstract class represents a firmware component.
 * 
 * @details The Firmware components update the fix diagnostics.
 */
class UbloxFirmware : public virtual ComponentInterface {
 public:
  /**
   * @brief Add the fix diagnostics to the updater.
   */
  void initializeRosDiagnostics();

 protected:
  /**
   * @brief Handle to send fix status to ROS diagnostics.
   */
  virtual void fixDiagnostic(
      diagnostic_updater::DiagnosticStatusWrapper& stat) = 0;
};

/**
 * @brief Implements functions for firmware version 6.
 */
class UbloxFirmware6 : public UbloxFirmware {
 public:
  UbloxFirmware6();

  /**
   * @brief Sets the fix status service type to GPS.
   */
  void getRosParams();

  /**
   * @brief Prints a warning, GNSS configuration not available in this version.
   * @return true if configured correctly, false otherwise
   */
  bool configureUblox();

  /**
   * @brief Subscribe to NavPVT, RxmRAW, and RxmSFRB messages. 
   */
  void subscribe();

 protected:
  /**
   * @brief Updates fix diagnostic from NavPOSLLH, NavVELNED, and NavSOL 
   * messages.
   */
  void fixDiagnostic(diagnostic_updater::DiagnosticStatusWrapper& stat);
  
 private:
  /**
   * @brief Publish a NavPOSLLh message & update the fix diagnostics & 
   * last known position.
   * @param m the message to publish
   */
  void publishNavPosLlh(const ublox_msgs::NavPOSLLH& m);
  
  /**
   * @brief Publish a NavVELNED message & update the last known velocity.
   * @param m the message to publish
   */
  void publishNavVelNed(const ublox_msgs::NavVELNED& m);

  /**
   * @brief Publish a NavSOL message and update the number of SVs used for the 
   * fix.
   * @param m the message to publish
   */
  void publishNavSol(const ublox_msgs::NavSOL& m);

  //! The last received navigation position
  ublox_msgs::NavPOSLLH last_nav_pos_; 
  //! The last received navigation velocity
  ublox_msgs::NavVELNED last_nav_vel_; 
  //! The last received num SVs used
  ublox_msgs::NavSOL last_nav_sol_; 
  //! The last NavSatFix based on last_nav_pos_
  sensor_msgs::NavSatFix fix_; 
  //! The last Twist based on last_nav_vel_
  geometry_msgs::TwistWithCovarianceStamped velocity_; 

  //! Used to configure NMEA (if set_nmea_) filled with ROS parameters
  ublox_msgs::CfgNMEA6 cfg_nmea_; 
  //! Whether or not to configure the NMEA settings
  bool set_nmea_; 
};

/**
 * @brief Abstract class for Firmware versions >= 7.
 * 
 * @details This class keeps track of the last NavPVT message uses it to 
 * update the fix diagnostics. It is a template class because the NavPVT message 
 * is a different length for firmware versions 7 and 8.
 *
 * @typedef NavPVT the NavPVT message type for the given firmware version
 */
template<typename NavPVT>
class UbloxFirmware7Plus : public UbloxFirmware {
 public:
  /**
   * @brief Publish a NavPVT message as well as NavSatFix and 
   * TwistWithCovarianceStamped messages.
   * 
   * @details This function also updates the fix diagnostics. If a fixed carrier 
   * phase solution is available, the NavSatFix status is set to GBAS fixed.
   * @param m the message to publish
   */
  void publishNavPvt(const NavPVT& m) {
    // NavPVT publisher
    static ros::Publisher publisher =
        nh->advertise<NavPVT>("navpvt", kROSQueueSize);
    publisher.publish(m);

    //
    // NavSatFix message
    //
    static ros::Publisher fixPublisher =
        nh->advertise<sensor_msgs::NavSatFix>("fix", kROSQueueSize);

    sensor_msgs::NavSatFix fix;
    fix.header.frame_id = frame_id;
    // set the timestamp
    uint8_t valid_time = m.VALID_DATE | m.VALID_TIME | m.VALID_FULLY_RESOLVED;
    if (m.valid & valid_time == valid_time 
        && m.flags2 & m.FLAGS2_CONFIRMED_AVAILABLE) {
      // Use NavPVT timestamp since it is valid
      fix.header.stamp.sec = toUtcSeconds(m);
      fix.header.stamp.nsec = m.nano;
    } else {
      // Use ROS time since NavPVT timestamp is not valid
      fix.header.stamp = ros::Time::now();
    }
    // Set the LLA
    fix.latitude = m.lat * 1e-7; // to deg
    fix.longitude = m.lon * 1e-7; // to deg
    fix.altitude = m.height * 1e-3; // to [m]
    // Set the Fix status
    bool fixOk = m.flags & m.FLAGS_GNSS_FIX_OK;
    if (fixOk && m.fixType >= m.FIX_TYPE_2D) {
      fix.status.status = fix.status.STATUS_FIX;
      if(m.flags & m.CARRIER_PHASE_FIXED)
        fix.status.status = fix.status.STATUS_GBAS_FIX;
    }
    else {
      fix.status.status = fix.status.STATUS_NO_FIX;
    }
    // Set the service based on GNSS configuration
    fix.status.service = fix_status_service;
    
    // Set the position covariance
    const double varH = pow(m.hAcc / 1000.0, 2); // to [m^2]
    const double varV = pow(m.vAcc / 1000.0, 2); // to [m^2]
    fix.position_covariance[0] = varH;
    fix.position_covariance[4] = varH;
    fix.position_covariance[8] = varV;
    fix.position_covariance_type =
        sensor_msgs::NavSatFix::COVARIANCE_TYPE_DIAGONAL_KNOWN;

    fixPublisher.publish(fix);

    //
    // Twist message
    //
    static ros::Publisher velocityPublisher =
        nh->advertise<geometry_msgs::TwistWithCovarianceStamped>("fix_velocity",
                                                                 kROSQueueSize);
    geometry_msgs::TwistWithCovarianceStamped velocity;
    velocity.header.stamp = fix.header.stamp;
    velocity.header.frame_id = frame_id;

    // convert to XYZ linear velocity [m/s] in ENU
    velocity.twist.twist.linear.x = m.velE * 1e-3;
    velocity.twist.twist.linear.y = m.velN * 1e-3;
    velocity.twist.twist.linear.z = -m.velD * 1e-3;
    // Set the covariance
    const double covSpeed = pow(m.sAcc * 1e-3, 2);
    const int cols = 6;
    velocity.twist.covariance[cols * 0 + 0] = covSpeed;
    velocity.twist.covariance[cols * 1 + 1] = covSpeed;
    velocity.twist.covariance[cols * 2 + 2] = covSpeed;
    velocity.twist.covariance[cols * 3 + 3] = -1;  //  angular rate unsupported

    velocityPublisher.publish(velocity);

    //
    // Update diagnostics 
    //
    last_nav_pvt_ = m;
    freq_diag->tick(fix.header.stamp);
    updater->update();
  }

 protected:

  /**
   * @brief Update the fix diagnostics from PVT message.
   */
  void fixDiagnostic(diagnostic_updater::DiagnosticStatusWrapper& stat) {
    // check the last message, convert to diagnostic
    if (last_nav_pvt_.fixType == 
        ublox_msgs::NavPVT::FIX_TYPE_DEAD_RECKONING_ONLY) {
      stat.level = diagnostic_msgs::DiagnosticStatus::WARN;
      stat.message = "Dead reckoning only";
    } else if (last_nav_pvt_.fixType == ublox_msgs::NavPVT::FIX_TYPE_2D) {
      stat.level = diagnostic_msgs::DiagnosticStatus::WARN;
      stat.message = "2D fix";
    } else if (last_nav_pvt_.fixType == ublox_msgs::NavPVT::FIX_TYPE_3D) {
      stat.level = diagnostic_msgs::DiagnosticStatus::OK;
      stat.message = "3D fix";
    } else if (last_nav_pvt_.fixType ==
               ublox_msgs::NavPVT::FIX_TYPE_GNSS_DEAD_RECKONING_COMBINED) {
      stat.level = diagnostic_msgs::DiagnosticStatus::OK;
      stat.message = "GPS and dead reckoning combined";
    } else if (last_nav_pvt_.fixType == 
               ublox_msgs::NavPVT::FIX_TYPE_TIME_ONLY) {
      stat.level = diagnostic_msgs::DiagnosticStatus::OK;
      stat.message = "Time only fix";
    }

    // If fix not ok (w/in DOP & Accuracy Masks), raise the diagnostic level
    if (!(last_nav_pvt_.flags & ublox_msgs::NavPVT::FLAGS_GNSS_FIX_OK)) {
      stat.level = diagnostic_msgs::DiagnosticStatus::WARN;
      stat.message += ", fix not ok";
    } 
    // Raise diagnostic level to error if no fix
    if (last_nav_pvt_.fixType == ublox_msgs::NavPVT::FIX_TYPE_NO_FIX) {
      stat.level = diagnostic_msgs::DiagnosticStatus::ERROR;
      stat.message = "No fix";
    }

    // append last fix position
    stat.add("iTOW [ms]", last_nav_pvt_.iTOW);
    stat.add("Latitude [deg]", last_nav_pvt_.lat * 1e-7);
    stat.add("Longitude [deg]", last_nav_pvt_.lon * 1e-7);
    stat.add("Altitude [m]", last_nav_pvt_.height * 1e-3);
    stat.add("Height above MSL [m]", last_nav_pvt_.hMSL * 1e-3);
    stat.add("Horizontal Accuracy [m]", last_nav_pvt_.hAcc * 1e-3);
    stat.add("Vertical Accuracy [m]", last_nav_pvt_.vAcc * 1e-3);
    stat.add("# SVs used", (int)last_nav_pvt_.numSV);
  }
 
  //! The last received NavPVT message
  NavPVT last_nav_pvt_; 
  // Whether or not to enable the given GNSS
  //! Whether or not to enable GPS
  bool enable_gps_; 
  //! Whether or not to enable GLONASS
  bool enable_glonass_; 
  //! Whether or not to enable QZSS
  bool enable_qzss_; 
  //! Whether or not to enable SBAS
  bool enable_sbas_; 
  //! The QZSS Signal configuration, see CfgGNSS message
  uint32_t qzss_sig_cfg_;
};

/**
 * @brief  Implements functions for firmware version 7.
 */
class UbloxFirmware7 : public UbloxFirmware7Plus<ublox_msgs::NavPVT7> {
 public:
  UbloxFirmware7();

  /**
   * @brief Get the parameters specific to firmware version 7.
   *
   * @details Get the GNSS and NMEA settings.
   */
  void getRosParams();
  
  /**
   * @brief Configure GNSS individually. Only configures GLONASS.
   */
  bool configureUblox();
  
  /**
   * @brief Subscribe to messages which are not generic to all firmware. 
   * 
   * @details Subscribe to NavPVT7 messages, RxmRAW, and RxmSFRB messages. 
   */
  void subscribe();
  
  private:
    //! Used to configure NMEA (if set_nmea_)
    /*! 
     * Filled from ROS parameters
     */
    ublox_msgs::CfgNMEA7 cfg_nmea_; 
    //! Whether or not to Configure the NMEA settings
    bool set_nmea_; 
};

/**
 *  @brief Implements functions for firmware version 8.
 */
class UbloxFirmware8 : public UbloxFirmware7Plus<ublox_msgs::NavPVT> {
 public:
  UbloxFirmware8();

  /**
   * @brief Get the ROS parameters specific to firmware version 8.
   *
   * @details Get the GNSS, NMEA, and UPD settings.
   */
  void getRosParams();
  
  /**
   * @brief Configure settings specific to firmware 8 based on ROS parameters.
   * 
   * @details Configure GNSS, if it is different from current settings.  
   * Configure the NMEA if desired by the user. It also may clear the 
   * flash memory based on the ROS parameters.
   */
  bool configureUblox();
  
  /**
   * @brief Subscribe to u-blox messages which are not generic to all firmware
   * versions. 
   * 
   * @details Subscribe to NavPVT, NavSAT, MonHW, and RxmRTCM messages based
   * on user settings.
   */
  void subscribe();

 private:
  // Set from ROS parameters
  //! Whether or not to enable the Galileo GNSS
  bool enable_galileo_; 
  //! Whether or not to enable the BeiDuo GNSS
  bool enable_beidou_; 
  //! Whether or not to enable the IMES GNSS
  bool enable_imes_; 
  //! Type of device reset after GNSS configuration. 
  /*!
   * Only used if GNSS configuration is changed.
   * See CfgRST message for constants.
   */
  uint8_t reset_mode_;
  //! Whether or not to configure the NMEA settings
  bool set_nmea_; 
  //! Desired NMEA configuration.
  ublox_msgs::CfgNMEA cfg_nmea_; 
  //! Whether to save the BBR to flash on shutdown
  bool save_on_shutdown_; 
  //! Whether to clear the flash memory during configuration
  bool clear_bbr_; 
};

/**
 * @brief Implements functions for Automotive Dead Reckoning (ADR) and 
 * Untethered Dead Reckoning (UDR) Devices.
 */
class UbloxAdrUdr: public virtual ComponentInterface {
 public:
  /**
   * @brief Get the ADR/UDR parameters.
   *
   * @details Get the use_adr parameter and check that the nav_rate is 1 Hz.
   */
  void getRosParams();

  /**
   * @brief Configure ADR/UDR settings.
   * @details Configure the use_adr setting.
   * @return true if configured correctly, false otherwise
   */
  bool configureUblox();

  /**
   * @brief Subscribe to ADR/UDR messages.
   *
   * @details Subscribe to NavATT, ESF and HNR messages based on user 
   * parameters.
   */
  void subscribe();

  /**
   * @brief Initialize the ROS diagnostics for the ADR/UDR device.
   * @todo unimplemented
   */
  void initializeRosDiagnostics() {
    ROS_WARN("ROS Diagnostics specific to U-Blox ADR/UDR devices is %s",
             "unimplemented. See UbloxAdrUdr class in node.h & node.cpp.");
  }

 protected:
  //! Whether or not to enable dead reckoning
  bool use_adr_; 
};

/**
 * @brief Implements functions for FTS products. Currently unimplemented.
 * @todo Unimplemented.
 */
class UbloxFts: public virtual ComponentInterface {
  /**
   * @brief Get the FTS parameters. 
   * @todo Currently unimplemented.
   */
  void getRosParams() {
    ROS_WARN("Functionality specific to U-Blox FTS devices is %s",
             "unimplemented. See UbloxFts class in node.h & node.cpp.");
  }

  /**
   * @brief Configure FTS settings. 
   * @todo Currently unimplemented.
   */
  bool configureUblox() {}

  /**
   * @brief Subscribe to FTS messages.
   * @todo Currently unimplemented.
   */
  void subscribe() {}

  /**
   * @brief Adds diagnostic updaters for FTS status. 
   * @todo Currently unimplemented.
   */
  void initializeRosDiagnostics() {}
};

/**
 * @brief Implements functions for High Precision GNSS Reference station 
 * devices.
 */
class UbloxHpgRef: public virtual ComponentInterface {
 public:
  /**
   * @brief Get the ROS parameters specific to the Reference Station 
   * configuration.
   *
   * @details Get the TMODE3 settings, the parameters it gets depends on the 
   * tmode3 parameter. For example, it will get survey-in parameters if the 
   * tmode3 parameter is set to survey in or it will get the fixed parameters if
   * it is set to fixed.
   */
  void getRosParams();

  /**
   * @brief Configure the u-blox Reference Station settings.
   *
   * @details Configure the TMODE3 settings and sets the internal state based
   * on the TMODE3 status. If the TMODE3 is set to fixed, it will configure
   * the RTCM messages.
   * @return true if configured correctly, false otherwise
   */
  bool configureUblox();

  /**
   * @brief Subscribe to u-blox Reference Station messages.
   *
   * @details Subscribe to NavSVIN messages based on user parameters.
   */
  void subscribe();

  /**
   * @brief Add diagnostic updaters for the TMODE3 status.
   */
  void initializeRosDiagnostics();

  /**
   * @brief Publish received Nav SVIN messages and updates diagnostics. 
   *
   * @details When the survey in finishes, it changes the measurement & 
   * navigation rate to the user configured values and enables the user 
   * configured RTCM messages.
   * @param m the message to publish
   */
  void publishNavSvIn(ublox_msgs::NavSVIN m);

 protected:
  /**
   * @brief Update the TMODE3 diagnostics.
   *
   * @details Updates the status of the survey-in if in  survey-in mode or the 
   * RTCM messages if in time mode.
   */
  void tmode3Diagnostics(diagnostic_updater::DiagnosticStatusWrapper& stat);

  /**
   * @brief Set the device mode to time mode (internal state variable).
   *
   * @details Configure the RTCM messages and measurement and navigation rate.
   */
  bool setTimeMode();

  //! The last received Nav SVIN message
  ublox_msgs::NavSVIN last_nav_svin_; 

  //! TMODE3 to set, such as disabled, survey-in, fixed
  uint8_t tmode3_; 
  
  // TMODE3 = Fixed mode settings
  //! True if coordinates are in LLA, false if ECEF
  /*! Used only for fixed mode */
  bool lla_flag_; 
  //! Antenna Reference Point Position [m] or [deg]
  /*! Used only for fixed mode */
  std::vector<float> arp_position_; 
  //! Antenna Reference Point Position High Precision [0.1 mm] or [deg * 1e-9]
  /*! Used only for fixed mode */
  std::vector<int8_t> arp_position_hp_; 
  //! Fixed Position Accuracy [m]
  /*! Used only for fixed mode */
  float fixed_pos_acc_; 
  
  // Settings for TMODE3 = Survey-in
  //! Whether to always reset the survey-in during configuration.
  /*! 
   * If false, it only resets survey-in if there's no fix and TMODE3 is 
   * disabled before configuration.
   * This variable is used only if TMODE3 is set to survey-in.
   */
  bool svin_reset_; 
  //! Measurement period used during Survey-In [s]
  /*! This variable is used only if TMODE3 is set to survey-in. */
  uint32_t sv_in_min_dur_; 
  //! Survey in accuracy limit [m]
  /*! This variable is used only if TMODE3 is set to survey-in. */
  float sv_in_acc_lim_; 

  //! Status of device time mode
  enum {
    INIT, //!< Initialization mode (before configuration)
    FIXED, //!< Fixed mode (should switch to time mode almost immediately)
    DISABLED, //!< Time mode disabled
    SURVEY_IN, //!< Survey-In mode
    TIME //!< Time mode, after survey-in or after configuring fixed mode
  } mode_;  
};

/**
 * @brief Implements functions for High Precision GNSS Rover devices.
 */
class UbloxHpgRov: public virtual ComponentInterface {
 public:
  // Constants for diagnostic updater
  //! Diagnostic updater: RTCM topic frequency min [Hz]
  const static double kRtcmFreqMin = 1; 
  //! Diagnostic updater: RTCM topic frequency max [Hz]
  const static double kRtcmFreqMax = 10; 
  //! Diagnostic updater: RTCM topic frequency tolerance [%]
  const static double kRtcmFreqTol = 0.1;
  //! Diagnostic updater: RTCM topic frequency window [num messages]
  const static int kRtcmFreqWindow = 25; 
  /**
   * @brief Get the ROS parameters specific to the Rover configuration.
   *
   * @details Get the DGNSS mode.
   */
  void getRosParams();

  /**
   * @brief Configure rover settings.
   *
   * @details Configure the DGNSS mode.
   * @return true if configured correctly, false otherwise
   */
  bool configureUblox();

  /**
   * @brief Subscribe to Rover messages, such as NavRELPOSNED.
   */
  void subscribe();

  /**
   * @brief Add diagnostic updaters for rover GNSS status, including
   * status of RTCM messages.
   */
  void initializeRosDiagnostics();

 protected:
  /**
   * @brief Update the rover diagnostics, including the carrier phase solution 
   * status (float or fixed).
   */
  void carrierPhaseDiagnostics(
      diagnostic_updater::DiagnosticStatusWrapper& stat);

  /**
   * @brief Publish received NavRELPOSNED messages.
   *
   * @details Saves the last received message and updates the rover diagnostics.
   */
  void publishNavRelPosNed(const ublox_msgs::NavRELPOSNED &m);


  //! Last relative position (used for diagnostic updater)
  ublox_msgs::NavRELPOSNED last_rel_pos_; 

  // For RTCM frequency diagnostic updater
  //! Diagnostic: RTCM frequency min (set from constant)
  double rtcm_freq_min; 
  //! Diagnostic: RTCM frequency max (set from constant)
  double rtcm_freq_max; 

  //! The DGNSS mode
  /*! see CfgDGNSS message for possible values */
  uint8_t dgnss_mode_; 

  // The RTCM topic frequency diagnostic updater
  boost::shared_ptr<diagnostic_updater::HeaderlessTopicDiagnostic> freq_rtcm_;
};

/**
 * @brief Implements functions for Time Sync products.
 * @todo partially implemented
 */
class UbloxTim: public virtual ComponentInterface {
  /**
   * @brief Get the Time Sync parameters. 
   * @todo Currently unimplemented.
   */
  void getRosParams() {
    ROS_WARN("Functionality specific to U-Blox TIM devices is only %s",
             "partially implemented. See UbloxTim class in node.h & node.cpp.");
  }

  /**
   * @brief Configure Time Sync settings.
   * @todo Currently unimplemented.
   */
  bool configureUblox() {}

  /**
   * @brief Subscribe to Time Sync messages.
   *
   * @details Subscribes to RxmRAWX & RxmSFRBX messages.
   */
  void subscribe();

  /**
   * @brief Adds diagnostic updaters for Time Sync status. 
   * @todo Currently unimplemented.
   */
  void initializeRosDiagnostics() {}
};

}

#endif