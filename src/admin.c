#include "admin.h"
#include "storage.h"

#include <string.h>

static int find_user(ParkyData *data, int id)
{
    int i;
    for (i = 0; i < data->userCount; ++i) {
        if (data->users[i].id == id) {
            return i;
        }
    }
    return -1;
}

static int find_parking(ParkyData *data, int id)
{
    int i;
    for (i = 0; i < data->parkingCount; ++i) {
        if (data->parkings[i].id == id) {
            return i;
        }
    }
    return -1;
}

static int find_complaint(ParkyData *data, int id)
{
    int i;
    for (i = 0; i < data->complaintCount; ++i) {
        if (data->complaints[i].id == id) {
            return i;
        }
    }
    return -1;
}

int admin_add_user(ParkyData *data, const User *user)
{
    if (data->userCount >= PARKY_MAX_USERS) {
        return 0;
    }
    data->users[data->userCount++] = *user;
    return storage_save(data);
}

int admin_update_user(ParkyData *data, const User *user)
{
    int index = find_user(data, user->id);
    if (index < 0) {
        return 0;
    }
    data->users[index] = *user;
    return storage_save(data);
}

int admin_delete_user(ParkyData *data, int userId)
{
    int index = find_user(data, userId);
    int i;
    if (index < 0) {
        return 0;
    }
    for (i = index; i < data->userCount - 1; ++i) {
        data->users[i] = data->users[i + 1];
    }
    --data->userCount;
    return storage_save(data);
}

int admin_get_user(const ParkyData *data, int userId, User *user)
{
    int i;
    for (i = 0; i < data->userCount; ++i) {
        if (data->users[i].id == userId) {
            *user = data->users[i];
            return 1;
        }
    }
    return 0;
}

int admin_add_parking(ParkyData *data, const Parking *parking)
{
    if (data->parkingCount >= PARKY_MAX_PARKINGS) {
        return 0;
    }
    data->parkings[data->parkingCount++] = *parking;
    return storage_save(data);
}

int admin_update_parking(ParkyData *data, const Parking *parking)
{
    int index = find_parking(data, parking->id);
    if (index < 0) {
        return 0;
    }
    data->parkings[index] = *parking;
    return storage_save(data);
}

int admin_delete_parking(ParkyData *data, int parkingId)
{
    int index = find_parking(data, parkingId);
    int i;
    if (index < 0) {
        return 0;
    }
    for (i = index; i < data->parkingCount - 1; ++i) {
        data->parkings[i] = data->parkings[i + 1];
    }
    --data->parkingCount;
    return storage_save(data);
}

int admin_add_agent(ParkyData *data, const User *agent)
{
    User value = *agent;
    value.role = USER_AGENT;
    if (data->userCount >= PARKY_MAX_USERS) {
        return 0;
    }
    data->users[data->userCount++] = value;
    return storage_save(data);
}

int admin_update_agent(ParkyData *data, const User *agent)
{
    int index = find_user(data, agent->id);
    if (index < 0 || data->users[index].role != USER_AGENT) {
        return 0;
    }
    data->users[index] = *agent;
    data->users[index].role = USER_AGENT;
    return storage_save(data);
}

int admin_delete_agent(ParkyData *data, int agentId)
{
    int index = find_user(data, agentId);
    int i;
    if (index < 0 || data->users[index].role != USER_AGENT) {
        return 0;
    }
    for (i = index; i < data->userCount - 1; ++i) {
        data->users[i] = data->users[i + 1];
    }
    --data->userCount;
    return storage_save(data);
}

int admin_assign_agent(ParkyData *data, int agentId, int parkingId)
{
    int userIndex = find_user(data, agentId);
    if (userIndex < 0 || data->users[userIndex].role != USER_AGENT ||
        find_parking(data, parkingId) < 0) {
        return 0;
    }
    data->users[userIndex].assignedParkingId = parkingId;
    return storage_save(data);
}

int admin_list_complaints(const ParkyData *data, Complaint *output, int maxCount)
{
    int count = data->complaintCount < maxCount ?
                data->complaintCount : maxCount;
    if (count > 0) {
        memcpy(output, data->complaints, (size_t)count * sizeof(Complaint));
    }
    return count;
}

int admin_resolve_complaint(ParkyData *data, int complaintId)
{
    int index = find_complaint(data, complaintId);
    if (index < 0) {
        return 0;
    }
    data->complaints[index].status = 2;
    return storage_save(data);
}

double admin_parking_occupancy_rate(const ParkyData *data, int parkingId)
{
    int i;
    for (i = 0; i < data->parkingCount; ++i) {
        if (data->parkings[i].id == parkingId) {
            if (data->parkings[i].totalSpots <= 0) {
                return 0.0;
            }
            return ((double)(data->parkings[i].totalSpots -
                             data->parkings[i].availableSpots) /
                    (double)data->parkings[i].totalSpots) * 100.0;
        }
    }
    return -1.0;
}
