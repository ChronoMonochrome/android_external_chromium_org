// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_sync_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_merge_result.h"
#include "sync/protocol/sync.pb.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/avatar/default_user_images.h"
#endif

using base::DictionaryValue;
using user_prefs::PrefRegistrySyncable;
using syncer::MANAGED_USERS;
using syncer::ModelType;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncChangeProcessor;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncError;
using syncer::SyncErrorFactory;
using syncer::SyncMergeResult;
using sync_pb::ManagedUserSpecifics;

namespace {

#if defined(OS_CHROMEOS)
const char kChromeOSAvatarPrefix[] = "chromeos-avatar-index:";
#else
const char kChromeAvatarPrefix[] = "chrome-avatar-index:";
#endif

SyncData CreateLocalSyncData(const std::string& id,
                             const std::string& name,
                             bool acknowledged,
                             const std::string& master_key,
                             const std::string& chrome_avatar,
                             const std::string& chromeos_avatar,
                             const std::string& password_signature_key,
                             const std::string& password_encryption_key) {
  ::sync_pb::EntitySpecifics specifics;
  specifics.mutable_managed_user()->set_id(id);
  specifics.mutable_managed_user()->set_name(name);
  if (!chrome_avatar.empty())
    specifics.mutable_managed_user()->set_chrome_avatar(chrome_avatar);
  if (!chromeos_avatar.empty())
    specifics.mutable_managed_user()->set_chromeos_avatar(chromeos_avatar);
  if (!master_key.empty())
    specifics.mutable_managed_user()->set_master_key(master_key);
  if (acknowledged)
    specifics.mutable_managed_user()->set_acknowledged(true);
  if (!password_signature_key.empty()) {
    specifics.mutable_managed_user()->
        set_password_signature_key(password_signature_key);
  }
  if (!password_encryption_key.empty()) {
    specifics.mutable_managed_user()->
        set_password_encryption_key(password_encryption_key);
  }
  return SyncData::CreateLocalData(id, name, specifics);
}

SyncData CreateSyncDataFromDictionaryEntry(const std::string& id,
                                           const base::Value& value) {
  const base::DictionaryValue* dict = NULL;
  bool success = value.GetAsDictionary(&dict);
  DCHECK(success);
  bool acknowledged = false;
  dict->GetBoolean(ManagedUserSyncService::kAcknowledged, &acknowledged);
  std::string name;
  dict->GetString(ManagedUserSyncService::kName, &name);
  DCHECK(!name.empty());
  std::string master_key;
  dict->GetString(ManagedUserSyncService::kMasterKey, &master_key);
  std::string chrome_avatar;
  dict->GetString(ManagedUserSyncService::kChromeAvatar, &chrome_avatar);
  std::string chromeos_avatar;
  dict->GetString(ManagedUserSyncService::kChromeOsAvatar, &chromeos_avatar);
  std::string signature;
  dict->GetString(ManagedUserSyncService::kPasswordSignatureKey, &signature);
  std::string encryption;
  dict->GetString(ManagedUserSyncService::kPasswordEncryptionKey, &encryption);

  return CreateLocalSyncData(id,
                             name,
                             acknowledged,
                             master_key,
                             chrome_avatar,
                             chromeos_avatar,
                             signature,
                             encryption);
}

}  // namespace

const char ManagedUserSyncService::kAcknowledged[] = "acknowledged";
const char ManagedUserSyncService::kChromeAvatar[] = "chromeAvatar";
const char ManagedUserSyncService::kChromeOsAvatar[] = "chromeOsAvatar";
const char ManagedUserSyncService::kMasterKey[] = "masterKey";
const char ManagedUserSyncService::kName[] = "name";
const char ManagedUserSyncService::kPasswordSignatureKey[] =
    "passwordSignatureKey";
const char ManagedUserSyncService::kPasswordEncryptionKey[] =
    "passwordEncryptionKey";
const int ManagedUserSyncService::kNoAvatar = -100;

ManagedUserSyncService::ManagedUserSyncService(PrefService* prefs)
    : prefs_(prefs) {
  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(
      prefs::kGoogleServicesLastUsername,
      base::Bind(&ManagedUserSyncService::OnLastSignedInUsernameChange,
                 base::Unretained(this)));
}

ManagedUserSyncService::~ManagedUserSyncService() {
}

// static
void ManagedUserSyncService::RegisterProfilePrefs(
    PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kManagedUsers,
                                   PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
bool ManagedUserSyncService::GetAvatarIndex(const std::string& avatar_str,
                                            int* avatar_index) {
  DCHECK(avatar_index);
  if (avatar_str.empty()) {
    *avatar_index = kNoAvatar;
    return true;
  }
#if defined(OS_CHROMEOS)
  const char* prefix = kChromeOSAvatarPrefix;
#else
  const char* prefix = kChromeAvatarPrefix;
#endif
  size_t prefix_len = strlen(prefix);
  if (avatar_str.size() <= prefix_len ||
      avatar_str.substr(0, prefix_len) != prefix) {
    return false;
  }

  if (!base::StringToInt(avatar_str.substr(prefix_len), avatar_index))
    return false;

  const int kChromeOSDummyAvatarIndex = -111;

#if defined(OS_CHROMEOS)
  return (*avatar_index == kChromeOSDummyAvatarIndex ||
          (*avatar_index >= chromeos::kFirstDefaultImageIndex &&
           *avatar_index < chromeos::kDefaultImagesCount));
#else
  // Check if the Chrome avatar index is set to a dummy value. Some early
  // supervised user profiles on ChromeOS stored a dummy avatar index as a
  // Chrome Avatar before there was logic to sync the ChromeOS avatar
  // separately. Handle this as if the Chrome Avatar was not chosen yet (which
  // is correct for these profiles).
  if (*avatar_index == kChromeOSDummyAvatarIndex)
    *avatar_index = kNoAvatar;
  return (*avatar_index == kNoAvatar ||
          (*avatar_index >= 0 &&
           static_cast<size_t>(*avatar_index) <
               profiles::GetDefaultAvatarIconCount()));
#endif
}

// static
std::string ManagedUserSyncService::BuildAvatarString(int avatar_index) {
#if defined(OS_CHROMEOS)
  const char* prefix = kChromeOSAvatarPrefix;
#else
  const char* prefix = kChromeAvatarPrefix;
#endif
  return base::StringPrintf("%s%d", prefix, avatar_index);
}

void ManagedUserSyncService::AddObserver(
    ManagedUserSyncServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void ManagedUserSyncService::RemoveObserver(
    ManagedUserSyncServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

scoped_ptr<base::DictionaryValue> ManagedUserSyncService::CreateDictionary(
    const std::string& name,
    const std::string& master_key,
    const std::string& signature_key,
    const std::string& encryption_key,
    int avatar_index) {
  scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->SetString(kName, name);
  result->SetString(kMasterKey, master_key);
  result->SetString(kPasswordSignatureKey, signature_key);
  result->SetString(kPasswordEncryptionKey, encryption_key);
  // TODO(akuegel): Get rid of the avatar stuff here when Chrome OS switches
  // to the avatar index that is stored as a shared setting.
  std::string chrome_avatar;
  std::string chromeos_avatar;
#if defined(OS_CHROMEOS)
  chromeos_avatar = BuildAvatarString(avatar_index);
#else
  chrome_avatar = BuildAvatarString(avatar_index);
#endif
  result->SetString(kChromeAvatar, chrome_avatar);
  result->SetString(kChromeOsAvatar, chromeos_avatar);
  return result.Pass();
}

void ManagedUserSyncService::AddManagedUser(const std::string& id,
                                            const std::string& name,
                                            const std::string& master_key,
                                            const std::string& signature_key,
                                            const std::string& encryption_key,
                                            int avatar_index) {
  UpdateManagedUserImpl(id,
                        name,
                        master_key,
                        signature_key,
                        encryption_key,
                        avatar_index,
                        true /* add */);
}

void ManagedUserSyncService::UpdateManagedUser(
    const std::string& id,
    const std::string& name,
    const std::string& master_key,
    const std::string& signature_key,
    const std::string& encryption_key,
    int avatar_index) {
  UpdateManagedUserImpl(id,
                        name,
                        master_key,
                        signature_key,
                        encryption_key,
                        avatar_index,
                        false /* update */);
}

void ManagedUserSyncService::UpdateManagedUserImpl(
    const std::string& id,
    const std::string& name,
    const std::string& master_key,
    const std::string& signature_key,
    const std::string& encryption_key,
    int avatar_index,
    bool add_user) {
  DictionaryPrefUpdate update(prefs_, prefs::kManagedUsers);
  base::DictionaryValue* dict = update.Get();
  scoped_ptr<base::DictionaryValue> value = CreateDictionary(
      name, master_key, signature_key, encryption_key, avatar_index);

  DCHECK_EQ(add_user, !dict->HasKey(id));
  base::DictionaryValue* entry = value.get();
  dict->SetWithoutPathExpansion(id, value.release());

  if (!sync_processor_)
    return;

  // If we're already syncing, create a new change and upload it.
  SyncChangeList change_list;
  change_list.push_back(
      SyncChange(FROM_HERE,
                 add_user ? SyncChange::ACTION_ADD : SyncChange::ACTION_UPDATE,
                 CreateSyncDataFromDictionaryEntry(id, *entry)));
  SyncError error =
      sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  DCHECK(!error.IsSet()) << error.ToString();
}

void ManagedUserSyncService::DeleteManagedUser(const std::string& id) {
  DictionaryPrefUpdate update(prefs_, prefs::kManagedUsers);
  bool success = update->RemoveWithoutPathExpansion(id, NULL);
  DCHECK(success);

  if (!sync_processor_)
    return;

  SyncChangeList change_list;
  change_list.push_back(SyncChange(
      FROM_HERE,
      SyncChange::ACTION_DELETE,
      SyncData::CreateLocalDelete(id, MANAGED_USERS)));
  SyncError sync_error =
      sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  DCHECK(!sync_error.IsSet());
}

const base::DictionaryValue* ManagedUserSyncService::GetManagedUsers() {
  DCHECK(sync_processor_);
  return prefs_->GetDictionary(prefs::kManagedUsers);
}

bool ManagedUserSyncService::UpdateManagedUserAvatarIfNeeded(
    const std::string& id,
    int avatar_index) {
  DictionaryPrefUpdate update(prefs_, prefs::kManagedUsers);
  base::DictionaryValue* dict = update.Get();
  DCHECK(dict->HasKey(id));
  base::DictionaryValue* value = NULL;
  bool success = dict->GetDictionaryWithoutPathExpansion(id, &value);
  DCHECK(success);

  bool acknowledged = false;
  value->GetBoolean(ManagedUserSyncService::kAcknowledged, &acknowledged);
  std::string name;
  value->GetString(ManagedUserSyncService::kName, &name);
  std::string master_key;
  value->GetString(ManagedUserSyncService::kMasterKey, &master_key);
  std::string signature;
  value->GetString(ManagedUserSyncService::kPasswordSignatureKey, &signature);
  std::string encryption;
  value->GetString(ManagedUserSyncService::kPasswordEncryptionKey, &encryption);
  std::string chromeos_avatar;
  value->GetString(ManagedUserSyncService::kChromeOsAvatar, &chromeos_avatar);
  std::string chrome_avatar;
  value->GetString(ManagedUserSyncService::kChromeAvatar, &chrome_avatar);
  // The following check is just for safety. We want to avoid that the existing
  // avatar selection is overwritten. Currently we don't allow the user to
  // choose a different avatar in the recreation dialog, anyway, if there is
  // already an avatar selected.
#if defined(OS_CHROMEOS)
  if (!chromeos_avatar.empty() && avatar_index != kNoAvatar)
    return false;
#else
  if (!chrome_avatar.empty() && avatar_index != kNoAvatar)
    return false;
#endif

  chrome_avatar = avatar_index == kNoAvatar ?
      std::string() : BuildAvatarString(avatar_index);
#if defined(OS_CHROMEOS)
  value->SetString(kChromeOsAvatar, chrome_avatar);
#else
  value->SetString(kChromeAvatar, chrome_avatar);
#endif

  if (!sync_processor_)
    return true;

  SyncChangeList change_list;
  change_list.push_back(SyncChange(
      FROM_HERE,
      SyncChange::ACTION_UPDATE,
      CreateLocalSyncData(id, name, acknowledged, master_key,
                          chrome_avatar, chromeos_avatar,
                          signature, encryption)));
  SyncError error =
      sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  DCHECK(!error.IsSet()) << error.ToString();
  return true;
}

void ManagedUserSyncService::ClearManagedUserAvatar(const std::string& id) {
  bool cleared = UpdateManagedUserAvatarIfNeeded(id, kNoAvatar);
  DCHECK(cleared);
}

void ManagedUserSyncService::GetManagedUsersAsync(
    const ManagedUsersCallback& callback) {
  // If we are already syncing, just run the callback.
  if (sync_processor_) {
    callback.Run(GetManagedUsers());
    return;
  }

  // Otherwise queue it up until we start syncing.
  callbacks_.push_back(callback);
}

void ManagedUserSyncService::Shutdown() {
  NotifyManagedUsersSyncingStopped();
}

SyncMergeResult ManagedUserSyncService::MergeDataAndStartSyncing(
    ModelType type,
    const SyncDataList& initial_sync_data,
    scoped_ptr<SyncChangeProcessor> sync_processor,
    scoped_ptr<SyncErrorFactory> error_handler) {
  DCHECK_EQ(MANAGED_USERS, type);
  sync_processor_ = sync_processor.Pass();
  error_handler_ = error_handler.Pass();

  SyncChangeList change_list;
  SyncMergeResult result(MANAGED_USERS);

  DictionaryPrefUpdate update(prefs_, prefs::kManagedUsers);
  base::DictionaryValue* dict = update.Get();
  result.set_num_items_before_association(dict->size());
  std::set<std::string> seen_ids;
  int num_items_added = 0;
  int num_items_modified = 0;
  for (SyncDataList::const_iterator it = initial_sync_data.begin();
       it != initial_sync_data.end(); ++it) {
    DCHECK_EQ(MANAGED_USERS, it->GetDataType());
    const ManagedUserSpecifics& managed_user =
        it->GetSpecifics().managed_user();
    base::DictionaryValue* value = new base::DictionaryValue();
    value->SetString(kName, managed_user.name());
    value->SetBoolean(kAcknowledged, managed_user.acknowledged());
    value->SetString(kMasterKey, managed_user.master_key());
    value->SetString(kChromeAvatar, managed_user.chrome_avatar());
    value->SetString(kChromeOsAvatar, managed_user.chromeos_avatar());
    value->SetString(kPasswordSignatureKey,
        managed_user.password_signature_key());
    value->SetString(kPasswordEncryptionKey,
        managed_user.password_encryption_key());
    if (dict->HasKey(managed_user.id()))
      num_items_modified++;
    else
      num_items_added++;
    dict->SetWithoutPathExpansion(managed_user.id(), value);
    seen_ids.insert(managed_user.id());
  }

  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    if (seen_ids.find(it.key()) != seen_ids.end())
      continue;

    change_list.push_back(
        SyncChange(FROM_HERE,
                   SyncChange::ACTION_ADD,
                   CreateSyncDataFromDictionaryEntry(it.key(), it.value())));
  }
  result.set_error(sync_processor_->ProcessSyncChanges(FROM_HERE, change_list));

  result.set_num_items_modified(num_items_modified);
  result.set_num_items_added(num_items_added);
  result.set_num_items_after_association(dict->size());

  DispatchCallbacks();

  return result;
}

void ManagedUserSyncService::StopSyncing(ModelType type) {
  DCHECK_EQ(MANAGED_USERS, type);
  // The observers may want to change the Sync data, so notify them before
  // resetting the |sync_processor_|.
  NotifyManagedUsersSyncingStopped();
  sync_processor_.reset();
  error_handler_.reset();
}

SyncDataList ManagedUserSyncService::GetAllSyncData(
    ModelType type) const {
  SyncDataList data;
  DictionaryPrefUpdate update(prefs_, prefs::kManagedUsers);
  base::DictionaryValue* dict = update.Get();
  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance())
    data.push_back(CreateSyncDataFromDictionaryEntry(it.key(), it.value()));

  return data;
}

SyncError ManagedUserSyncService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& change_list) {
  SyncError error;
  DictionaryPrefUpdate update(prefs_, prefs::kManagedUsers);
  base::DictionaryValue* dict = update.Get();
  for (SyncChangeList::const_iterator it = change_list.begin();
       it != change_list.end(); ++it) {
    SyncData data = it->sync_data();
    DCHECK_EQ(MANAGED_USERS, data.GetDataType());
    const ManagedUserSpecifics& managed_user =
        data.GetSpecifics().managed_user();
    switch (it->change_type()) {
      case SyncChange::ACTION_ADD:
      case SyncChange::ACTION_UPDATE: {
        // Every item we get from the server should be acknowledged.
        DCHECK(managed_user.acknowledged());
        const base::DictionaryValue* old_value = NULL;
        dict->GetDictionaryWithoutPathExpansion(managed_user.id(), &old_value);

        // For an update action, the managed user should already exist, for an
        // add action, it should not.
        DCHECK_EQ(
            old_value ? SyncChange::ACTION_UPDATE : SyncChange::ACTION_ADD,
            it->change_type());

        // If the managed user switched from unacknowledged to acknowledged,
        // we might need to continue with a registration.
        if (old_value && !old_value->HasKey(kAcknowledged))
          NotifyManagedUserAcknowledged(managed_user.id());

        base::DictionaryValue* value = new base::DictionaryValue;
        value->SetString(kName, managed_user.name());
        value->SetBoolean(kAcknowledged, managed_user.acknowledged());
        value->SetString(kMasterKey, managed_user.master_key());
        value->SetString(kChromeAvatar, managed_user.chrome_avatar());
        value->SetString(kChromeOsAvatar, managed_user.chromeos_avatar());
        value->SetString(kPasswordSignatureKey,
                         managed_user.password_signature_key());
        value->SetString(kPasswordEncryptionKey,
                         managed_user.password_encryption_key());
        dict->SetWithoutPathExpansion(managed_user.id(), value);

        NotifyManagedUsersChanged();
        break;
      }
      case SyncChange::ACTION_DELETE: {
        DCHECK(dict->HasKey(managed_user.id())) << managed_user.id();
        dict->RemoveWithoutPathExpansion(managed_user.id(), NULL);
        break;
      }
      case SyncChange::ACTION_INVALID: {
        NOTREACHED();
        break;
      }
    }
  }
  return error;
}

void ManagedUserSyncService::OnLastSignedInUsernameChange() {
  DCHECK(!sync_processor_);

  // If the last signed in user changes, we clear all data, to avoid managed
  // users from one custodian appearing in another one's profile.
  prefs_->ClearPref(prefs::kManagedUsers);
}

void ManagedUserSyncService::NotifyManagedUserAcknowledged(
    const std::string& managed_user_id) {
  FOR_EACH_OBSERVER(ManagedUserSyncServiceObserver, observers_,
                    OnManagedUserAcknowledged(managed_user_id));
}

void ManagedUserSyncService::NotifyManagedUsersSyncingStopped() {
  FOR_EACH_OBSERVER(ManagedUserSyncServiceObserver, observers_,
                    OnManagedUsersSyncingStopped());
}

void ManagedUserSyncService::NotifyManagedUsersChanged() {
  FOR_EACH_OBSERVER(ManagedUserSyncServiceObserver,
                    observers_,
                    OnManagedUsersChanged());
}

void ManagedUserSyncService::DispatchCallbacks() {
  const base::DictionaryValue* managed_users =
      prefs_->GetDictionary(prefs::kManagedUsers);
  for (std::vector<ManagedUsersCallback>::iterator it = callbacks_.begin();
       it != callbacks_.end(); ++it) {
    it->Run(managed_users);
  }
  callbacks_.clear();
}
