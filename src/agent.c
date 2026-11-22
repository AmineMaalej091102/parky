#include "agent.h"
#include "storage.h"

#include <stdio.h>
#include <string.h>

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

static int find_service(ParkyData *data, int id)
{
    int i;
    for (i = 0; i < data->serviceCount; ++i) {
        if (data->services[i].id == id) {
            return i;
        }
    }
    return -1;
}

static int find_reservation(ParkyData *data, int id)
{
    int i;
    for (i = 0; i < data->reservationCount; ++i) {
        if (data->reservations[i].id == id) {
            return i;
        }
    }
    return -1;
}

int agent_set_spot_status(ParkyData *data, int parkingId, int status)
{
    int index = find_parking(data, parkingId);
    if (index < 0 || status < PARKING_AVAILABLE || status > PARKING_RESERVED) {
        return 0;
    }
    data->parkings[index].status = status;
    if (status == PARKING_AVAILABLE) {
        data->parkings[index].availableSpots =
            data->parkings[index].totalSpots;
    } else if (status == PARKING_OCCUPIED) {
        data->parkings[index].availableSpots = 0;
    }
    return storage_save(data);
}

int agent_add_service(ParkyData *data, const Service *service)
{
    if (data->serviceCount >= PARKY_MAX_SERVICES) {
        return 0;
    }
    data->services[data->serviceCount++] = *service;
    return storage_save(data);
}

int agent_update_service(ParkyData *data, const Service *service)
{
    int index = find_service(data, service->id);
    if (index < 0) {
        return 0;
    }
    data->services[index] = *service;
    return storage_save(data);
}

int agent_delete_service(ParkyData *data, int serviceId)
{
    int index = find_service(data, serviceId);
    int i;
    if (index < 0) {
        return 0;
    }
    for (i = index; i < data->serviceCount - 1; ++i) {
        data->services[i] = data->services[i + 1];
    }
    --data->serviceCount;
    return storage_save(data);
}

int agent_resolve_reservation(ParkyData *data, int reservationId, int approved)
{
    int index = find_reservation(data, reservationId);
    if (index < 0) {
        return 0;
    }
    data->reservations[index].status =
        approved ? RESERVATION_APPROVED : RESERVATION_REJECTED;
    return storage_save(data);
}

int agent_submit_complaint(ParkyData *data, int agentId,
                           const char *subject, const char *description)
{
    Complaint complaint;
    int nextId = 1;
    int i;

    if (data->complaintCount >= PARKY_MAX_COMPLAINTS) {
        return 0;
    }
    for (i = 0; i < data->complaintCount; ++i) {
        if (data->complaints[i].id >= nextId) {
            nextId = data->complaints[i].id + 1;
        }
    }
    memset(&complaint, 0, sizeof(complaint));
    complaint.id = nextId;
    complaint.citizenId = agentId;
    snprintf(complaint.subject, sizeof(complaint.subject), "%s", subject);
    snprintf(complaint.description, sizeof(complaint.description), "%s",
             description);
    complaint.status = 0;
    data->complaints[data->complaintCount++] = complaint;
    return storage_save(data);
}
