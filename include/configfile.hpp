#ifndef __CLASS_PROXYSQL_CONFIGFILE_H
#define __CLASS_PROXYSQL_CONFIGFILE_H

//#include "proxysql.h"
//#include "cpp.h"

#include "libconfig.h++"

using namespace libconfig;


class ProxySQL_ConfigFile {
  private:
  struct stat statbuf;
  Config cfg;
  char *filename;
  public:
  ProxySQL_ConfigFile();
  bool OpenFile(const char *);
	bool ReadGlobals();
	bool configVariable(const char *, const char *, int &, int, int, int, int);
	bool configVariable(const char *, const char *, int64_t &, int64_t, int64_t, int64_t, int64_t);
	bool configVariable(const char *, const char *, bool &, bool);
	bool configVariable(const char *, const char *, char **, const char *);
  ~ProxySQL_ConfigFile();
};


#endif /* __CLASS_PROXYSQL_CONFIGFILE_H */
