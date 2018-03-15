/*
 * Copyright 2018 Google
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIRESTORE_CORE_SRC_FIREBASE_FIRESTORE_LOCAL_LEVELDB_TRANSACTION_H_
#define FIRESTORE_CORE_SRC_FIREBASE_FIRESTORE_LOCAL_LEVELDB_TRANSACTION_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include <absl/strings/string_view.h>
#include <leveldb/db.h>

#if __OBJC__
#import <Protobuf/GPBProtocolBuffers.h>
#endif

namespace firebase {
namespace firestore {
namespace local {

/**
 * LevelDBTransaction tracks pending changes to entries in leveldb, including
 * deletions. It also provides an Iterator to traverse a merged view of pending
 * changes and committed values.
 */
class LevelDBTransaction {
  using Deletions = std::set<std::string>;
  using Mutations = std::map<std::string, std::string>;

 public:
  /**
   * Iterator iterates over a merged view of pending changes from the
   * transaction and any unchanged values in the underlying leveldb instance.
   */
  class Iterator {
   public:
    explicit Iterator(LevelDBTransaction* txn);

    /**
     * Returns true if this iterator points to an entry
     */
    bool Valid();

    /**
     * Seeks this iterator to the first key equal to or greater than the given
     * key
     */
    void Seek(const std::string& key);

    /**
     * Advances the iterator to the next entry
     */
    void Next();

    /**
     * Returns the key of the current entry
     */
    absl::string_view key();

    /**
     * Returns the value of the current entry
     */
    absl::string_view value();

   private:
    /**
     * Advances to the next non-deleted key in leveldb.
     */
    void AdvanceLDB();

    /**
     * Returns true if the given slice matches a key present in the deletions_
     * set.
     */
    bool IsDeleted(leveldb::Slice slice);

    /**
     * Syncs with the underlying transaction. If the transaction has been
     * updated, the mutation iterator may need to be reset. Returns true if this
     * resulted in moving to a new underlying entry (i.e. the entry represented
     * by current_ was deleted).
     */
    bool SyncToTransaction();

    /**
     * Given the current state of the internal iterators, set is_valid_,
     * is_mutation_, and current_.
     */
    void UpdateCurrent();

    std::unique_ptr<leveldb::Iterator> db_iter_;

    // The last observed version of the underlying transaction
    int32_t last_version_;
    // The underlying transaction.
    LevelDBTransaction* txn_;
    Mutations::iterator mutations_iter_;
    // We save the current key and value so that once an iterator is Valid(), it
    // remains so at least until the next call to Seek() or Next(), even if the
    // underlying data is deleted.
    std::pair<std::string, std::string> current_;
    // True if current_ represents an entry in the mutations_ map, rather than
    // committed data.
    bool is_mutation_;
    // True if the iterator pointed to a valid entry the last time Next() or
    // Seek() was called.
    bool is_valid_;
  };

  explicit LevelDBTransaction(
      leveldb::DB* db,
      const leveldb::ReadOptions& readOptions = DefaultReadOptions(),
      const leveldb::WriteOptions& writeOptions = DefaultWriteOptions());

  LevelDBTransaction(const LevelDBTransaction& other) = delete;

  LevelDBTransaction& operator=(const LevelDBTransaction& other) = delete;

  /**
   * Returns a default set of ReadOptions
   */
  static const leveldb::ReadOptions& DefaultReadOptions();

  /**
   * Returns a default set of WriteOptions
   */
  static const leveldb::WriteOptions& DefaultWriteOptions();

  /**
   * Remove the database entry (if any) for "key".  It is not an error if "key"
   * did not exist in the database.
   */
  void Delete(const absl::string_view& key);

#if __OBJC__
  /**
   * Schedules the row identified by `key` to be set to the given protocol
   * buffer message when this transaction commits.
   */
  void Put(const absl::string_view& key, GPBMessage* message) {
    NSData* data = [message data];
    std::string key_string(key);
    mutations_[key_string] = std::string((const char*)data.bytes, data.length);
    version_++;
  }
#endif

  /**
   * Schedules the row identified by `key` to be set to `value` when this
   * transaction commits.
   */
  void Put(const absl::string_view& key, const absl::string_view& value);

  /**
   * Sets the contents of `value` to the latest known value for the given key,
   * including any pending mutations and `Status::OK` is returned. If the key
   * doesn't exist in leveldb, or it is scheduled for deletion in this
   * transaction, `Status::NotFound` is returned.
   */
  leveldb::Status Get(const absl::string_view& key, std::string* value);

  /**
   * Returns a new Iterator over the pending changes in this transaction, merged
   * with the existing values already in leveldb.
   */
  Iterator* NewIterator();

  /**
   * Commits the transaction. All pending changes are written. The transaction
   * should not be used after calling this method.
   */
  void Commit();

 private:
  leveldb::DB* db_;
  Mutations mutations_;
  Deletions deletions_;
  leveldb::ReadOptions read_options_;
  leveldb::WriteOptions write_options_;
  int32_t version_;
};

}  // namespace local
}  // namespace firestore
}  // namespace firebase

#endif  // FIRESTORE_CORE_SRC_FIREBASE_FIRESTORE_LOCAL_LEVELDB_TRANSACTION_H_
