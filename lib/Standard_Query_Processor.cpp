#include <iostream>     // std::cout
#include <algorithm>    // std::sort
#include <vector>       // std::vector
#include "re2/re2.h"
#include "re2/regexp.h"
#include "proxysql.h"
#include "cpp.h"



#define QUERY_PROCESSOR_VERSION "0.1.728"



struct __RE2_objects_t {
	re2::RE2::Options *opt;
	RE2 *re;
};

typedef struct __RE2_objects_t re2_t;

static bool rules_sort_comp_function (QP_rule_t * a, QP_rule_t * b) { return (a->rule_id < b->rule_id); }

static re2_t * compile_query_rule(QP_rule_t *qr) {
	re2_t *r=(re2_t *)malloc(sizeof(re2_t));
	r->opt=new re2::RE2::Options(RE2::Quiet);
	r->opt->set_case_sensitive(false);
	r->re=new RE2(qr->match_pattern, *r->opt);
	return r;
};

static void __delete_query_rule(QP_rule_t *qr) {
	proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 5, "Deleting rule in %p : rule_id:%d, active:%d, username=%s, schemaname=%s, flagIN:%d, %smatch_pattern=\"%s\", flagOUT:%d replace_pattern=\"%s\", destination_hostgroup:%d, apply:%d\n", qr, qr->rule_id, qr->active, qr->username, qr->schemaname, qr->flagIN, (qr->negate_match_pattern ? "(!)" : "") , qr->match_pattern, qr->flagOUT, qr->replace_pattern, qr->destination_hostgroup, qr->apply);
	if (qr->username)
		free(qr->username);
	if (qr->schemaname)
		free(qr->schemaname);
	if (qr->match_pattern)
		free(qr->match_pattern);
	if (qr->replace_pattern)
		free(qr->replace_pattern);
	if (qr->regex_engine) {
		re2_t *r=(re2_t *)qr->regex_engine;
		delete r->opt;
		delete r->re;
		free(qr->regex_engine);
	}
	free(qr);
};

// delete all the query rules in a Query Processor Table
// Note that this function is called by GloQPro with &rules (generic table)
//     and is called by each mysql thread with _thr_SQP_rules (per thread table)
static void __reset_rules(std::vector<QP_rule_t *> * qrs) {
	proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 5, "Resetting rules in Query Processor Table %p\n", qrs);
	if (qrs==NULL) return;
	QP_rule_t *qr;
	for (std::vector<QP_rule_t *>::iterator it=qrs->begin(); it!=qrs->end(); ++it) {
		qr=*it;
		__delete_query_rule(qr);
	}
	qrs->clear();
}


// per thread variables
__thread unsigned int _thr_SQP_version;
__thread std::vector<QP_rule_t *> * _thr_SQP_rules;

class Standard_Query_Processor: public Query_Processor {

private:
rwlock_t rwlock;
std::vector<QP_rule_t *> rules;

volatile unsigned int version;
protected:

public:
Standard_Query_Processor() {
	proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 4, "Initializing Query Processor with version=0\n");
	spinlock_rwlock_init(&rwlock);
	version=0;
};

virtual ~Standard_Query_Processor() {
	__reset_rules(&rules);
};

// This function is called by each thread when it starts. It create a Query Processor Table for each thread
virtual void init_thread() {
	proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 4, "Initializing Per-Thread Query Processor Table with version=0\n");
	_thr_SQP_version=0;
	_thr_SQP_rules=new std::vector<QP_rule_t *>;
};


virtual void end_thread() {
	proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 4, "Destroying Per-Thread Query Processor Table with version=%d\n", _thr_SQP_version);
	__reset_rules(_thr_SQP_rules);
	delete _thr_SQP_rules;
};

virtual void print_version() {
	fprintf(stderr,"Standard Query Processor rev. %s -- %s -- %s\n", QUERY_PROCESSOR_VERSION, __FILE__, __TIMESTAMP__);
};

virtual void wrlock() {
	spin_wrlock(&rwlock);
};

virtual void wrunlock() {
	spin_wrunlock(&rwlock);
};



virtual QP_rule_t * new_query_rule(int rule_id, bool active, char *username, char *schemaname, int flagIN, char *match_pattern, bool negate_match_pattern, int flagOUT, char *replace_pattern, int destination_hostgroup, int cache_ttl, bool apply) {
	QP_rule_t * newQR=(QP_rule_t *)malloc(sizeof(QP_rule_t));
	newQR->rule_id=rule_id;
	newQR->active=active;
	newQR->username=(username ? strdup(username) : NULL);
	newQR->schemaname=(schemaname ? strdup(schemaname) : NULL);
	newQR->flagIN=flagIN;
	newQR->match_pattern=(match_pattern ? strdup(match_pattern) : NULL);
	newQR->negate_match_pattern=negate_match_pattern;
	newQR->flagOUT=flagOUT;
	newQR->replace_pattern=(replace_pattern ? strdup(replace_pattern) : NULL);
	newQR->destination_hostgroup=destination_hostgroup;
	newQR->cache_ttl=cache_ttl;
	newQR->apply=apply;
	newQR->regex_engine=NULL;
	newQR->hits=0;
	proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 5, "Creating new rule in %p : rule_id:%d, active:%d, username=%s, schemaname=%s, flagIN:%d, %smatch_pattern=\"%s\", flagOUT:%d replace_pattern=\"%s\", destination_hostgroup:%d, apply:%d\n", newQR, newQR->rule_id, newQR->active, newQR->username, newQR->schemaname, newQR->flagIN, (newQR->negate_match_pattern ? "(!)" : "") , newQR->match_pattern, newQR->flagOUT, newQR->replace_pattern, newQR->destination_hostgroup, newQR->apply);
	return newQR;
};


virtual void delete_query_rule(QP_rule_t *qr) {
	__delete_query_rule(qr);
/*
	if (qr->username)
		free(qr->username);
	if (qr->schemaname)
		free(qr->schemaname);
	if (qr->match_pattern)
		free(qr->match_pattern);
	if (qr->replace_pattern)
		free(qr->replace_pattern);
	free(qr);
*/
};

virtual void reset_all(bool lock) {
	if (lock) spin_wrlock(&rwlock);
/*
	QP_rule_t *qr;
	for (std::vector<QP_rule_t *>::iterator it=rules.begin(); it!=rules.end(); ++it) {
		qr=*it;
		__delete_query_rule(qr);
	}
	rules.clear();
*/
	__reset_rules(&rules);
	if (lock) spin_wrunlock(&rwlock);
};

virtual bool insert(QP_rule_t *qr, bool lock) {
	bool ret=true;
	if (lock) spin_wrlock(&rwlock);
	rules.push_back(qr);
	if (lock) spin_wrunlock(&rwlock);
	return ret;
};


virtual void sort(bool lock) {
	if (lock) spin_wrlock(&rwlock);
	proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 4, "Sorting rules\n");
	std::sort (rules.begin(), rules.end(), rules_sort_comp_function);
	if (lock) spin_wrunlock(&rwlock);
};

// when commit is called, the version number is increased and the this will trigger the mysql threads to get a new Query Processor Table
// The operation is asynchronous
virtual void commit() {
	spin_wrlock(&rwlock);
	__sync_add_and_fetch(&version,1);
	proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 4, "Increasing version number to %d - all threads will notice this and refresh their rules\n", version);
	spin_wrunlock(&rwlock);
};


virtual QP_out_t * process_mysql_query(MySQL_Session *sess, void *ptr, unsigned int size, bool delete_original) {
	QP_out_t *ret=NULL;
	unsigned int len=size-sizeof(mysql_hdr)-1;
	char *query=(char *)l_alloc(len+1);
	memcpy(query,(char *)ptr+sizeof(mysql_hdr)+1,len);
	query[len]=0;
	if (__sync_add_and_fetch(&version,0) > _thr_SQP_version) {
		// update local rules;
		proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 4, "Detected a changed in version. Global:%d , local:%d . Refreshing...\n", version, _thr_SQP_version);
		spin_rdlock(&rwlock);
		_thr_SQP_version=__sync_add_and_fetch(&version,0);
		__reset_rules(_thr_SQP_rules);
		QP_rule_t *qr1;
		QP_rule_t *qr2;
		for (std::vector<QP_rule_t *>::iterator it=rules.begin(); it!=rules.end(); ++it) {
			qr1=*it;
			if (qr1->active) {
				proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 4, "Copying Query Rule id: %d\n", qr1->rule_id);
				qr2=new_query_rule(qr1->rule_id, qr1->active, qr1->username, qr1->schemaname, qr1->flagIN, qr1->match_pattern, qr1->negate_match_pattern, qr1->flagOUT, qr1->replace_pattern, qr1->destination_hostgroup, qr1->cache_ttl, qr1->apply);
				if (qr2->match_pattern) {
					proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 4, "Compiling regex for rule_id: %d, match_pattern: \n", qr2->rule_id, qr2->match_pattern);
					qr2->regex_engine=(void *)compile_query_rule(qr2);
				}
				_thr_SQP_rules->push_back(qr2);
			}
		}
		spin_rdunlock(&rwlock); // unlock should be after the copy
	}
	QP_rule_t *qr;
	re2_t *re2p;
	int flagIN=0;
	for (std::vector<QP_rule_t *>::iterator it=_thr_SQP_rules->begin(); it!=_thr_SQP_rules->end(); ++it) {
		qr=*it;
		if (qr->flagIN != flagIN) {
			proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 6, "query rule %d has no matching flagIN\n", qr->rule_id);
			continue;
		}
		if (qr->username) {
			if (strcmp(qr->username,sess->userinfo_client.username)!=0) {
				proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 5, "query rule %d has no matching username\n", qr->rule_id);
				continue;
			}
		}
		if (qr->schemaname) {
			if (strcmp(qr->schemaname,sess->userinfo_client.schemaname)!=0) {
				proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 5, "query rule %d has no matching schemaname\n", qr->rule_id);
				continue;
			}
		}

		re2p=(re2_t *)qr->regex_engine;
		if (qr->match_pattern) {
			bool rc;
			if (ret && ret->new_query) {
				// if we already rewrote the query, process the new query
				//std::string *s=ret->new_query;
				rc=RE2::PartialMatch(ret->new_query->c_str(),*re2p->re);
			} else {
				// we never rewrote the query
				rc=RE2::PartialMatch(query,*re2p->re);
			}
			if ((rc==true && qr->negate_match_pattern==true) || ( rc==false && qr->negate_match_pattern==false )) {
				proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 5, "query rule %d has no matching pattern\n", qr->rule_id);
				continue;
			}
		}
		// if we arrived here, we have a match
		if (ret==NULL) {
			proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 5, "this is the first time we find a match\n");
			// create struct
			ret=(QP_out_t *)l_alloc(sizeof(QP_out_t));
			// initalized all values
			ret->ptr=NULL;
			ret->size=0;
			ret->destination_hostgroup=-1;
			ret->cache_ttl=-1;
			ret->new_query=NULL;
		}
		//__sync_fetch_and	_add(&qr->hits,1);
		qr->hits++; // this is done without atomic function because it updates only the local variables
			//ret=(QP_out_t *)malloc(sizeof(QP_out_t));

		if (qr->flagOUT >= 0) {
			proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 5, "query rule %d has changed flagOUT\n", qr->rule_id);
			flagIN=qr->flagOUT;
			//sess->query_info.flagOUT=flagIN;
    }
    if (qr->cache_ttl >= 0) {
			// Note: negative TTL means this rule doesn't change 
      proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 5, "query rule %d has set cache_ttl: %d. Query will%s hit the cache\n", qr->rule_id, qr->cache_ttl, (qr->cache_ttl == 0 ? " NOT" : "" ));
      ret->cache_ttl=qr->cache_ttl;
    }
    if (qr->destination_hostgroup >= 0) {
			// Note: negative TTL means this rule doesn't change 
      proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 5, "query rule %d has set destination hostgroup: %d\n", qr->rule_id, qr->destination_hostgroup);
      ret->destination_hostgroup=qr->destination_hostgroup;
    }

		if (qr->replace_pattern) {
			proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 5, "query rule %d on match_pattern \"%s\" has a replace_pattern \"%s\" to apply\n", qr->rule_id, qr->match_pattern, qr->replace_pattern);
			if (ret->new_query==NULL) ret->new_query=new std::string(query);
			RE2::Replace(ret->new_query,qr->match_pattern,qr->replace_pattern);
		}

		if (qr->apply==true) {
			proxy_debug(PROXY_DEBUG_MYSQL_QUERY_PROCESSOR, 5, "query rule %d is the last one to apply: exit!\n", qr->rule_id);
			goto __exit_process_mysql_query;
		}
	}
	
__exit_process_mysql_query:
	// FIXME : there is too much data being copied around
	l_free(len+1,query);
	return ret;
};

// this function is called by mysql_session to free the result generated by process_mysql_query()
virtual void delete_QP_out(QP_out_t *o) {
	l_free(sizeof(QP_out_t),o);
};

};

extern "C" Query_Processor * create_Query_Processor_func() {
    return new Standard_Query_Processor();
}

extern "C" void destroy_Query_Processor(Query_Processor * qp) {
    delete qp;
}
