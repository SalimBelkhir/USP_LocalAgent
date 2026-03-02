#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_action.h>
//#include <amxb/amxb.h>
//#include <amxo/amxo.h>

#define COLOR_RESET  "\033[0m"
#define COLOR_BLUE   "\033[1;34m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_CYAN   "\033[1;36m"

#define UNUSED __attribute__((unused))
#define autodelete __attribute__((cleanup(free_stack)))

__attribute__ ((always_inline))
inline void free_stack(void *ptr) {
    amxc_var_delete(*(void **) ptr);
}

typedef struct _cpu_state {
	uint64_t user;
	uint64_t nice;
	uint64_t system;
	uint64_t idle;
	uint64_t iowait;
	uint64_t irq;
	uint64_t softirq;
} cpu_state_t;

typedef struct {
	unsigned long total_kb;
	unsigned long free_kb;
} mem_info_t;

static int read_os_release_field(const char* key,char* buf,size_t len) {
	FILE* fp = fopen("/etc/os-release","r");
	if(fp == NULL)
		return -1;
	char line[256];
	size_t key_len = strlen(key);
	int found = -1;

	while(fgets(line,sizeof(line),fp) != NULL) {
		if(strncmp(line,key,key_len)== 0 && line[key_len] == '=') {
			char* value = line + key_len + 1;
			if(*value == '"')
				value++;
			size_t vlen = strlen(value);
			if(vlen > 0 && value[vlen-1] == '\n') { value[--vlen] = '\0'; }
            		if(vlen > 0 && value[vlen-1] == '"')  { value[--vlen] = '\0'; }

            		strncpy(buf, value, len - 1);
           		buf[len - 1] = '\0';
            		found = 0;
           		break;
		}
	}
	
	fclose(fp);
	return found;
}

amxd_status_t _get_end_point_id(amxd_object_t* object,
				amxd_param_t* param,
			    amxd_action_t reason,
				const amxc_var_t *const args,
				amxc_var_t* const retval,
				void* priv)
{
	amxd_status_t status = amxd_status_unknown_error;

	if(reason != action_param_read){
		    status = amxd_status_function_not_implemented;
		    goto exit;
	}

	status = amxd_action_param_read(object,param,reason,args,retval,priv);
	if(status != amxd_status_ok)
		    goto exit;

	amxc_var_set(cstring_t,retval,"os::prpl:rpi-001");
	status = amxd_status_ok;

exit:
	return status;
}

amxd_status_t _get_software_ver(amxd_object_t* object,
				amxd_param_t* param,
				amxd_action_t reason,
				const amxc_var_t* const args,
				amxc_var_t* const retval,
				void* priv)
{
	amxd_status_t status = amxd_status_unknown_error;
	char buf[64] = {0};

	if(reason != action_param_read){
		    status = amxd_status_function_not_implemented;
		    goto exit;
	}

	status = amxd_action_param_read(object,param,reason,args,retval,priv);
    if(status != amxd_status_ok)
            goto exit;
	
	if(read_os_release_field("VERSION_ID",buf,sizeof(buf)) == 0) {
		    amxc_var_set(cstring_t,retval,buf);
		    status = amxd_status_ok;
		    goto exit;
	}else {
		    status = amxd_status_invalid_value;
		    goto exit;
	}
exit:
	return status;
}


amxd_status_t _get_uptime(amxd_object_t* object,
                          amxd_param_t* param,
                          amxd_action_t reason,
                          const amxc_var_t* const args,
                          amxc_var_t* const retval,
                          void* priv)
{
	amxd_status_t status = amxd_status_unknown_error;
	FILE *fp = NULL; 
	double uptime_s = 0.0;

	if(reason != action_param_read){
                status = amxd_status_function_not_implemented;
                goto exit;
        }

    status = amxd_action_param_read(object,param,reason,args,retval,priv);
    if(status != amxd_status_ok)
                goto exit;

	fp = fopen("/proc/uptime","r");

	if(fp == NULL) {
                amxc_var_set(uint32_t,retval,0);
		status = amxd_status_unknown_error;
                goto exit;
	}

	if(fscanf(fp,"%lf",&uptime_s) != 1 ) {
		amxc_var_set(uint32_t,retval,0);
		status = amxd_status_unknown_error;
		goto exit;
	}

	amxc_var_set(uint32_t,retval,(uint32_t)uptime_s);
	status = amxd_status_ok;
	
exit:
	if(fp != NULL)
	    fclose(fp);
	return status;
}

static void cpu_info_fill_usage_data(cpu_state_t* data, char* line, size_t len) {
    amxc_string_t str_line;
    amxc_var_t parts;
    uint32_t index = 0;
    uint64_t* fields[7];
    fields[0] = &data->user;
    fields[1] = &data->nice;
    fields[2] = &data->system;
    fields[3] = &data->idle;
    fields[4] = &data->iowait;
    fields[5] = &data->irq;
    fields[6] = &data->softirq;

    amxc_var_init(&parts);
    amxc_string_init(&str_line, 0);
    amxc_string_push_buffer(&str_line, line, len);

    amxc_string_split(&str_line, &parts, NULL, NULL);

    amxc_var_for_each(part, (&parts)) {
        *fields[index] = amxc_var_dyncast(uint64_t, part);
        index++;
        if(index > 6) {
            break;
        }
    }

    amxc_string_take_buffer(&str_line);
    amxc_var_clean(&parts);
    amxc_string_clean(&str_line);
}

static char* cpu_info_build_key(int32_t id) {
    char* retval = NULL;
    amxc_string_t key;
    amxc_string_init(&key, 16);

    if(id < 0) {
        amxc_string_setf(&key, "cpu");
    } else {
        amxc_string_setf(&key, "cpu%d", id);
    }
    retval = amxc_string_take_buffer(&key);

    amxc_string_clean(&key);
    return retval;
}

static int read_cpu_stat(cpu_state_t* stat,int32_t cpu_id)
{
	int ret = -1;
	FILE* fp = NULL;
	ssize_t read = 0;
	char* line = NULL;
	size_t len = 0;
	char* key = cpu_info_build_key(cpu_id);
	size_t key_len = strlen(key);
	fp = fopen("/proc/stat","r");
	if(fp == NULL)
		goto exit;
	
	read = getline(&line,&len,fp);
	while(read != -1){
		if(strncmp(key,line,key_len)==0)
			break;
		read = getline(&line,&len,fp);
	}
	if(read == -1)
		goto exit;
	cpu_info_fill_usage_data(stat,line+key_len,len-key_len);
	ret = 0;

exit:
	if(fp != NULL)
		fclose(fp);
	free(key);
	free(line);
	return ret;
}

static uint32_t compute_cpu_usage(void) {
	cpu_state_t s1 = {0}, s2 = {0};
	if(read_cpu_stat(&s1,-1) != 0)
		return 0;
	usleep(100000);
	if(read_cpu_stat(&s2,-1) !=0)
		return 0;

	uint64_t idle1 = s1.idle + s1.iowait;
	uint64_t idle2 = s2.idle + s2.iowait;
	uint64_t total1 = s1.user + s1.nice + s1.system + s1.idle 
			+ s1.iowait + s1.irq  + s1.softirq;
	uint64_t total2 = s2.user + s2.nice + s2.system + s2.idle
			+ s2.iowait + s2.irq  + s2.softirq;
	uint64_t delta_idle = idle2 - idle1;
	uint64_t delta_total = total2 - total1;

	if (delta_total == 0)
		return 0;
	return (uint32_t)(100ULL * (delta_total - delta_idle) / delta_total);

}

amxd_status_t _get_cpu_usage(amxd_object_t* object,
			    amxd_param_t* param,
			    amxd_action_t reason,
			    const amxc_var_t* const args,
			    amxc_var_t* const retval,
			    void* priv)
{
	amxd_status_t status = amxd_status_unknown_error;
	if(reason != action_param_read){
		status = amxd_status_function_not_implemented;
		goto exit;
	}

	status = amxd_action_param_read(object, param, reason, args, retval, priv);
    	if(status != amxd_status_ok)
        	goto exit;
	
	amxc_var_set(uint32_t,retval,compute_cpu_usage());
	status = amxd_status_ok;

exit:
	return status;
}

static int read_mem_info(mem_info_t *m) {
	int rv = -1;
	FILE *fp = fopen("/proc/meminfo","r");
	if (fp == NULL )
		return rv ;

	char line[128] = {0};
	int got_total = 0;
	int got_free = 0;

	while(fgets(line,sizeof(line),fp) != NULL) {
		if(sscanf(line,"MemTotal: %lu kB",&m->total_kb) == 1 )
			got_total = 1;
		if(sscanf(line,"MemFree: %lu kB",&m->free_kb) == 1 )
			got_free = 1;
		if (got_total && got_free)
			break;
	}

	rv = (got_total && got_free) ? 0 : -1 ;

	fclose(fp);
	return rv;
}

amxd_status_t _get_memory_total(amxd_object_t *object,
				amxd_param_t *param,
				amxd_action_t reason,
				const amxc_var_t *const args,
				amxc_var_t *const retval,
				void *priv)
{
        amxd_status_t status = amxd_status_unknown_error;
	    mem_info_t m = {0};
        if(reason != action_param_read){
                status = amxd_status_function_not_implemented;
                goto exit;
        }

        status = amxd_action_param_read(object, param, reason, args, retval, priv);
        if(status != amxd_status_ok)
                goto exit;

	if(read_mem_info(&m) == -1)
		goto exit;

	amxc_var_set(uint32_t,retval,(uint32_t)m.total_kb);
	status = amxd_status_ok;
exit:
	return status;
}

amxd_status_t _get_memory_free(amxd_object_t *object,
                                amxd_param_t *param,
                                amxd_action_t reason,
                                const amxc_var_t *const args,
                                amxc_var_t *const retval,
                                void *priv)
{
    amxd_status_t status = amxd_status_unknown_error;
    mem_info_t m = {0};
    if(reason != action_param_read){
            status = amxd_status_function_not_implemented;
            goto exit;
    }

    status = amxd_action_param_read(object, param, reason, args, retval, priv);
    if(status != amxd_status_ok)
            goto exit;

	if (read_mem_info(&m) == -1 )
	       	goto exit;

	amxc_var_set(uint32_t,retval,(uint32_t)m.free_kb);
	status = amxd_status_ok;

exit:
	return status;
}

static void on_watched_path_changed(const char* const sig_name,
									const amxc_var_t* const data,
				    				void* const priv) {
	const char* recipient = (const char*)priv;
	const char* path = NULL ;
	amxc_var_t* var_path = amxc_var_get_key(data,"path",AMXC_VAR_FLAG_DEFAULT);

	if(var_path != NULL)
		path = amxc_var_constcast(cstring_t,var_path);

	printf(COLOR_BLUE "USP INFO : ValueChange detected\n" COLOR_RESET);
        printf(COLOR_YELLOW "  Signal   : %s\n" COLOR_RESET, sig_name ? sig_name : "?");
        printf(COLOR_YELLOW "  Path     : %s\n" COLOR_RESET, path     ? path     : "?");
        printf(COLOR_YELLOW"  Recipient: %s\n" COLOR_RESET, recipient ? recipient : "?");
}

void _la_subscription_added(UNUSED const char* const sig_name,
			   const amxc_var_t* const data,
			   UNUSED void* const priv){
	amxc_var_t* var_index = amxc_var_get_key(data,"index",AMXC_VAR_FLAG_DEFAULT);
	if(var_index == NULL){
		        printf(COLOR_YELLOW "[USP] subscription_added no index in event data\n" COLOR_RESET);
			return ;
	}
	uint32_t index = amxc_var_constcast(uint32_t,var_index);
	//index got the index of data (e.g. index = 1)
	/*
	 * data coming from an event looks like:
		 {
		     "path": "Device.LocalAgent.Subscription.1.",
		     "index": 1,
		     "name": "cpe-1"
		 }
	 * */

	amxd_dm_t* dm = (amxd_dm_t*)priv;
	amxc_var_t* var_path = amxc_var_get_key(data,"path",AMXC_VAR_FLAG_DEFAULT);

        if(var_path == NULL || dm == NULL) {
        	printf(COLOR_YELLOW "[USP] subscription_added : cannot get dm or path\n" COLOR_RESET);
        	return;
    	}
	
	//got the dm(root of data model) and path of data
	/*
	 *dm (root)	
	  └── Device.
   	      └── LocalAgent.          <- amxd_dm_findf(dm, "Device.LocalAgent.")
        	├── Subscription.
        	│   └── 1.           <- amxd_dm_findf(dm, "Device
					.LocalAgent.Subscription.1.")
        	└── Controller.
	 * */
	const char* templ_path = amxc_var_constcast(cstring_t, var_path);
	
	amxd_object_t* templ = amxd_dm_findf(dm,"%s",templ_path);
	if(templ == NULL) {
	        printf("[USP] subscription_added:template object not found: %s\n",templ_path);
        	return;
        }
	//templ got the object of path of the data(templ = Device.LocalAgent.Subscription.)
	
	amxd_object_t* inst = amxd_object_get_instance(templ,NULL,index);
	if(inst == NULL) {
		 printf("[USP] subscription_added: instance %u not found\n", index);
       		 return;
        }
	// we get inst = Device.LocalAgent.Subscription.{index}
	
	//reading parameter from instance
	autodelete amxc_var_t* enable_var;
    autodelete amxc_var_t* recipient_var;
    autodelete amxc_var_t* reflist_var;
    autodelete amxc_var_t* id_var;
    autodelete amxc_var_t* ttl_var;
    autodelete amxc_var_t* persistent_var;
    autodelete amxc_var_t* trigger_action_var;

   	amxc_var_new(&enable_var);
    amxc_var_new(&recipient_var);
    amxc_var_new(&reflist_var);
    amxc_var_new(&id_var);
    amxc_var_new(&ttl_var);
    amxc_var_new(&persistent_var);
    amxc_var_new(&trigger_action_var);


	amxd_object_get_param(inst,"Enable",enable_var);
	amxd_object_get_param(inst,"Recipient",recipient_var);
	amxd_object_get_param(inst,"ReferenceList",reflist_var);
	amxd_object_get_param(inst,"ID",id_var);
	amxd_object_get_param(inst,"TimeToLive",ttl_var);
	amxd_object_get_param(inst,"Persistent",persistent_var);
	amxd_object_get_param(inst, "TriggerAction", trigger_action_var);

	bool enable = amxc_var_constcast(bool, enable_var);
	bool persistent = amxc_var_constcast(bool,persistent_var);

    if(!enable){
	    printf("[USP] Subscription.%u is disabled, skipping\n", index);
		return;
	}
	if(!persistent){
	    printf("[USP] Subscription.%u is not persistet, skipping\n",index);
		return;
	}

    const char* recipient  = amxc_var_constcast(cstring_t, recipient_var);
    const char* reflist    = amxc_var_constcast(cstring_t, reflist_var);
	const char* id         = amxc_var_constcast(cstring_t, id_var);
    const uint32_t ttl     = amxc_var_constcast(uint32_t,ttl_var);
	const char* trigger_action = amxc_var_constcast(cstring_t, trigger_action_var);	

	if (ttl == 0) printf("[USP] TimeToLive invalide value\n");

	if(trigger_action == NULL ||
   	       (strcmp(trigger_action, "Notify") != 0 &&
    	    strcmp(trigger_action, "Config") != 0 &&
            strcmp(trigger_action, "NotifyAndConfig") != 0)) {
            printf("[USP] Subscription.%u trigger action is invalid\n", index);
            return;
        }

	printf("[USP] Subscription.%u added\n", index);
    printf("  Recipient    : %s\n", recipient ? recipient : "(none)");
    printf("  ReferenceList: %s\n", reflist   ? reflist   : "(none)");
	printf("  ID           :%s\n", id ? id : "(none)");	

	if(reflist == NULL || *reflist == '\0') {
        	printf("[USP] ReferenceList is empty, nothing to watch\n");
			return;
    }

	if(recipient == NULL) {
    		printf("[USP] recipient is NULL, skipping\n");
            return;
    }
    char* recipient_copy = strdup(recipient);

    if(recipient_copy == NULL) {
            printf("[USP] strdup failed, skipping\n");
            return;
        }


	amxc_string_t ref_str;
	amxc_string_init(&ref_str,0);
	amxc_string_set(&ref_str,reflist);

	amxc_llist_t paths;
	amxc_llist_init(&paths);
	amxc_string_split_to_llist(&ref_str,&paths,',');

	amxc_llist_for_each(it,&paths) {
		amxc_string_t* path_str = amxc_container_of(it,amxc_string_t,it);
		//amxc_string_t is a typedef struct not a simple type
		amxc_string_trim(path_str,NULL);
	    const char* watch_path = amxc_string_get(path_str,0);

		if(watch_path == NULL || *watch_path == '\0')
			continue;
		printf("  Watching: %s\n", watch_path);
	
		amxp_slot_connect(NULL,
				 "dm:object-changed",
				 watch_path,
			 	 on_watched_path_changed,
				 (void*) recipient_copy);	 
	}

	amxc_llist_clean(&paths,amxc_string_list_it_free);
	amxc_string_clean(&ref_str);

}

int _entry(int reason, UNUSED amxd_dm_t *dm,UNUSED amxo_parser_t *parser) {
	//TODO : Treat reason argument cases  
	return 0;
}
