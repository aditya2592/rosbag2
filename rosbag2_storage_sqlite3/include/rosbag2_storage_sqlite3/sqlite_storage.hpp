// Copyright 2018, Bosch Software Innovations GmbH.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ROSBAG2_STORAGE_SQLITE3__SQLITE_STORAGE_HPP_
#define ROSBAG2_STORAGE_SQLITE3__SQLITE_STORAGE_HPP_

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "rcpputils/thread_safety_annotations.hpp"
#include "rosbag2_storage/storage_interfaces/read_write_interface.hpp"
#include "rosbag2_storage/serialized_bag_message.hpp"
#include "rosbag2_storage/storage_filter.hpp"
#include "rosbag2_storage/topic_metadata.hpp"
#include "rosbag2_storage/bag_metadata.hpp"
#include "rosbag2_storage_sqlite3/sqlite_wrapper.hpp"
#include "rosbag2_storage_sqlite3/visibility_control.hpp"

// This is necessary because of using stl types here. It is completely safe, because
// a) the member is not accessible from the outside
// b) there are no inline functions.
#ifdef _WIN32
# pragma warning(push)
# pragma warning(disable:4251)
#endif

namespace rosbag2_storage_plugins
{

class ROSBAG2_STORAGE_DEFAULT_PLUGINS_PUBLIC SqliteStorage
  : public rosbag2_storage::storage_interfaces::ReadWriteInterface
{
public:
  SqliteStorage() = default;

  ~SqliteStorage() override;

  void open(
    const rosbag2_storage::StorageOptions & storage_options,
    rosbag2_storage::storage_interfaces::IOFlag io_flag =
    rosbag2_storage::storage_interfaces::IOFlag::READ_WRITE) override;

  void update_metadata(const rosbag2_storage::BagMetadata & metadata) override;

  void remove_topic(const rosbag2_storage::TopicMetadata & topic) override;

  void create_topic(const rosbag2_storage::TopicMetadata & topic) override;

  void write(std::shared_ptr<const rosbag2_storage::SerializedBagMessage> message) override;

  void write(
    const std::vector<std::shared_ptr<const rosbag2_storage::SerializedBagMessage>> & messages)
  override;

  bool set_read_order(const rosbag2_storage::ReadOrder &) override;

  bool has_next() override;

  std::shared_ptr<rosbag2_storage::SerializedBagMessage> read_next() override;

  std::vector<rosbag2_storage::TopicMetadata> get_all_topics_and_types() override;

  rosbag2_storage::BagMetadata get_metadata() override;

  std::string get_relative_file_path() const override;

  uint64_t get_bagfile_size() const override;

  std::string get_storage_identifier() const override;

  uint64_t get_minimum_split_file_size() const override;

  void set_filter(const rosbag2_storage::StorageFilter & storage_filter) override;

  void reset_filter() override;

  void seek(const rcutils_time_point_value_t & timestamp) override;

  std::string get_storage_setting(const std::string & key);

  /// Return the sqlite database wrapper.
  /**
   * \throws std::runtime_error if open() has not been called
   */
  SqliteWrapper & get_sqlite_database_wrapper();

  int get_db_schema_version() const;
  std::string get_recorded_ros_distro() const;

  enum class PresetProfile
  {
    Resilient,
    WriteOptimized,
  };
  static PresetProfile parse_preset_profile(const std::string & profile_string);

private:
  void initialize();
  void read_metadata();
  void prepare_for_writing();
  void prepare_for_reading();
  void fill_topics_and_types();
  void activate_transaction();
  void commit_transaction();
  void write_locked(std::shared_ptr<const rosbag2_storage::SerializedBagMessage> message)
  RCPPUTILS_TSA_REQUIRES(database_write_mutex_);
  int get_last_rowid();
  int read_db_schema_version();

  using ReadQueryResult = SqliteStatementWrapper::QueryResult<
    std::shared_ptr<rcutils_uint8_array_t>, rcutils_time_point_value_t, std::string, int>;

  std::shared_ptr<SqliteWrapper> database_ RCPPUTILS_TSA_GUARDED_BY(database_write_mutex_);
  SqliteStatement write_statement_ {};
  SqliteStatement read_statement_ {};
  ReadQueryResult message_result_ {nullptr};
  ReadQueryResult::Iterator current_message_row_ {
    nullptr, SqliteStatementWrapper::QueryResult<>::Iterator::POSITION_END};
  std::unordered_map<std::string, int> topics_ RCPPUTILS_TSA_GUARDED_BY(database_write_mutex_);
  std::vector<rosbag2_storage::TopicMetadata> all_topics_and_types_;
  std::string relative_path_;
  std::atomic_bool active_transaction_ {false};

  rcutils_time_point_value_t seek_time_ = 0;
  int seek_row_id_ = 0;
  rosbag2_storage::ReadOrder read_order_{};
  rosbag2_storage::StorageFilter storage_filter_ {};
  rosbag2_storage::storage_interfaces::IOFlag storage_mode_{
    rosbag2_storage::storage_interfaces::IOFlag::READ_WRITE};

  // This mutex is necessary to protect:
  // a) database access (this could also be done with FULLMUTEX), but see b)
  // b) topics_ collection - since we could be writing and reading it at the same time
  std::mutex database_write_mutex_;

  const int kDBSchemaVersion_ = 3;
  int db_schema_version_ = -1;  //  Valid version number starting from 1
  rosbag2_storage::BagMetadata metadata_{};
};


}  // namespace rosbag2_storage_plugins

#ifdef _WIN32
# pragma warning(pop)
#endif

#endif  // ROSBAG2_STORAGE_SQLITE3__SQLITE_STORAGE_HPP_
