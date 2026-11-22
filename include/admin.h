#ifndef ADMIN_H
#define ADMIN_H

#include "models.h"

int admin_add_user(ParkyData *data, const User *user);
int admin_update_user(ParkyData *data, const User *user);
int admin_delete_user(ParkyData *data, int userId);
int admin_get_user(const ParkyData *data, int userId, User *user);

int admin_add_parking(ParkyData *data, const Parking *parking);
int admin_update_parking(ParkyData *data, const Parking *parking);
int admin_delete_parking(ParkyData *data, int parkingId);

int admin_add_agent(ParkyData *data, const User *agent);
int admin_update_agent(ParkyData *data, const User *agent);
int admin_delete_agent(ParkyData *data, int agentId);
int admin_assign_agent(ParkyData *data, int agentId, int parkingId);

int admin_list_complaints(const ParkyData *data, Complaint *output, int maxCount);
int admin_resolve_complaint(ParkyData *data, int complaintId);

double admin_parking_occupancy_rate(const ParkyData *data, int parkingId);

#endif
