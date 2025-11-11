#ifndef DEFAULT_JSON_H
#define DEFAULT_JSON_H


void set_default_manager_json(char *source);
void set_default_kenmec_json(char *source);
void set_default_ethernet_json(char *source);
void set_default_eth0_json(char *source);

void cjson_deep_merge(cJSON *root, cJSON *patch);

#endif