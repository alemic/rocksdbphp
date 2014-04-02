/**
 *  rocksdbphp.cpp
 *  app RocksDB
 *  implementation fast key/value storage RocksDB for php
 *  @author valmat <ufabiz@gmail.com>
 *  @github https://github.com/valmat/rocksdbphp
 */



#include <string>
#include <iostream>
#include <phpcpp.h>
#include "rocksdb/db.h"
#include "rocksdb/write_batch.h"
#include "rocksdb/merge_operator.h"

//#include <php5/Zend/zend_iterators.h>
/**
 *  PHP includes
 */
//#include <php.h>
//#include "zend_iterators.h"



//using namespace std;

namespace RocksDBPHP {
	class Int64Incrementor : public rocksdb::AssociativeMergeOperator {
	 public:
	    virtual bool Merge(
	        const rocksdb::Slice& key,
	        const rocksdb::Slice* existing_value,
	        const rocksdb::Slice& value,
	        std::string* new_value,
	        rocksdb::Logger* logger) const override {

	        // assuming 0 if no existing value
	        int64_t existing = 0;
	        if (existing_value) {
	             existing = std::strtoull(existing_value->ToString().c_str(), nullptr, 10);
	        }

	        int64_t incr = std::strtoull(value.ToString().c_str(),0,10);
	        int64_t newval = existing + incr;
	        *new_value = std::to_string(newval);
	        return true;        // always return true for this, since we treat all errors as "zero".
	    }

	    virtual const char* Name() const override {
	        return "Int64Incrementor";
	    }
	};
//}

//namespace RocksDBPHP {
	class Driver : public Php::Base//, public zend_object_iterator
	{
	private:
	    /**
	     * db
	     */
	    rocksdb::DB* db;

	    /**
	     * db path distantion
	     */
	    std::string dbpath;

	    /**
	     * db path distantion
	     */
	    rocksdb::Options dboptions;

	    /**
	     * last opreation status
	     */
	    rocksdb::Status status;
    


	public:

	    Driver() = default;// noexcept

	    /**
	     * function __construct
	     * @param string path
	     * @param bool create_if_missing
	     */
	    virtual void __construct(Php::Parameters &params) {

	        if (params.size() < 1) {
	            throw Php::Exception("Requires parameter path");
	            return;
	        }
	        this->dbpath = params[0].stringValue();
	        bool create_if_missing = true;
	        if (params.size() > 1) {
	            create_if_missing = (bool)params[1];
	        }

	        this->dboptions.create_if_missing = create_if_missing;
	        this->dboptions.merge_operator.reset(new Int64Incrementor);
	        this->status = rocksdb::DB::Open(this->dboptions, this->dbpath, &this->db);
	        std::cerr << this->status.ToString() << std::endl;

	        if (!this->status.ok())  {
	            throw Php::Exception(   this->status.ToString()  );
	        }
	    }

	    virtual ~Driver() {
	        //delete Incrementor; //not required : std::shared_ptr
	        delete db;
	        std::cout << "~Driver()" << std::endl;
	    }
	    virtual void __destruct() {}

	    /**
	     * set value by key
	     * function set
	     * @param string key
	     * @param string value
	     * @return bool rezult
	     */
	    Php::Value set(Php::Parameters &params) {
	        if (params.size() < 2) {
	            throw Php::Exception("Requires 2 parameters: key and value");
	            return false;
	        }
	        std::string key, value;
	        key   = params[0].stringValue();
	        value = params[1].stringValue();
	        //std::cout << "set("<< key << ")=" << value << std::endl;

	        this->status = db->Put(rocksdb::WriteOptions(), key, value);
	        return (bool)(this->status.ok());
	    }

	    /**
	     * set array values by array keys
	     * function mset
	     * @param Php::Array keys
	     * @param Php::Array values
	     * @return bool rezult status
	     */
	    Php::Value mset(Php::Parameters &params) {
	        if (params.size() < 2) {
	            throw Php::Exception("Requires 2 parameter: array keys and array values");
	            return false;
	        }
	        if(!params[0].isArray() || !params[1].isArray() ) {
	            throw Php::Exception("Required parameters are arrays");
	            return false;
	        }
	        if(params[0].size() != params[1].size() ) {
	            throw Php::Exception("Dimensions do not match");
	            return false;
	        }

	        unsigned int arrSize = params[0].size();
	        rocksdb::WriteBatch batch;
	        std::string key, val;

	        for (unsigned int i = 0; i < arrSize; i++) {
	            key = params[0][i].value().stringValue();
	            val = params[1][i].value().stringValue();
	            
	            //std::cout << "set("<< key << ")=" << val << std::endl;
	            batch.Put(key, val);
	        }
	        this->status = db->Write(rocksdb::WriteOptions(), &batch);

	        return (bool)(this->status.ok());
	    }

	    /**
	     * get value by key
	     * function get
	     * @param string key
	     * @return string value or NULL (if not key exist)
	     */
	    Php::Value get(Php::Parameters &params) {
	        if (params.size() < 1) {
	            throw Php::Exception("Requires 1 parameters: key");
	            return false;
	        }
	        std::string key, value;
	        key   = params[0].stringValue();
	        const Php::Value  phpnull;

	        this->status = db->Get(rocksdb::ReadOptions(), key, &value);
	        if (!this->status.ok()) {
	            return phpnull;
	        }
	        return value;
	    }

	    /**
	     * get array values by array keys
	     * function mget
	     * @param Php::Array keys
	     * @return Php::Array rezult
	     */
	    Php::Value mget(Php::Parameters &params) {
	        if (params.size() < 1) {
	            throw Php::Exception("Requires 1 parameters: array keys");
	            return false;
	        }
	        if(!params[0].isArray()) {
	            throw Php::Exception("Requires array");
	            return false;
	        }

	        Php::Value rez;
	        const Php::Value  phpnull;
	        unsigned int arrSize = params[0].size();

	        std::vector<std::string> ks;
	        ks.reserve(arrSize);

	        // Slice keys that only refers to string ks!
	        std::vector<rocksdb::Slice> keys;
	        keys.reserve(arrSize);

	        std::vector<std::string> values(arrSize);
	        std::vector<rocksdb::Status> statuses(arrSize);

	        for (unsigned int i = 0; i < arrSize; i++) {
	            ks.push_back( params[0][i].value().stringValue() );
	            keys.push_back( ks[i] );
	        }
	        statuses = db->MultiGet(rocksdb::ReadOptions(), keys, &values);
	        for (unsigned int i = 0; i < arrSize; i++) {
	            rez[ks[i]] = (statuses[i].ok()) ? (Php::Value) values[i] : phpnull;
	        }
	        return rez;
	    }

	    /**
	     * fast check exist key
	     * function isset
	     * @param string key
	     * @return bool (true if key exist)
	     */
	    Php::Value isset(Php::Parameters &params) {
	        unsigned int sz = params.size();

	        if (sz < 1) {
	            throw Php::Exception("Requires 1 parameters: key");
	            return false;
	        }
	        std::string key, value;
	        key   = params[0].stringValue();
	        bool r;

	        r = db->KeyMayExist(rocksdb::ReadOptions(), key, &value);
	        //It is not guaranteed that value will be determined
	        //std::cout << "Get(" << key <<") = " << value << std::endl;
	        //std::cout << "sz" << sz << std::endl;

	        /*
	        if (r && sz > 1) {
	            params[1] = (Php::Value) value;
	        }
	        */

	        return r;
	    }

	    /**
	     * remove key from db
	     * function del
	     * @param string key
	     * @return bool status rezult
	     */
	    Php::Value del(Php::Parameters &params) {
	        if (params.size() < 1) {
	            throw Php::Exception("Requires 1 parameters: key");
	            return false;
	        }

	        this->status = db->Delete(rocksdb::WriteOptions(), params[0].stringValue());
	        return (bool)(this->status.ok());
	    }

	    /**
	     * remove keys from db
	     * function mdel
	     * @param Php::Array keys
	     * @return bool status rezult
	     */
	    Php::Value mdel(Php::Parameters &params) {
	        if (params.size() < 1) {
	            throw Php::Exception("Requires 1 parameters: array keys");
	            return false;
	        }
	        if(!params[0].isArray()) {
	            throw Php::Exception("Requires array");
	            return false;
	        }

	        unsigned int arrSize = params[0].size();
	        rocksdb::WriteBatch batch;
	        std::string key;

	        for (unsigned int i = 0; i < arrSize; i++) {
	            key = params[0][i].value().stringValue();
	            batch.Delete(key);
	        }
	        this->status = db->Write(rocksdb::WriteOptions(), &batch);

	        return (bool)(this->status.ok());
	    }

	    /**
	     * function getStatus
	     * @param void
	     * @return string status.ToString()
	     */
	    Php::Value getStatus() {
	        return  this->status.ToString();
	    }

	    /**
	     * function incr
	     * @param string key
	     * @param int incval, default: 1
	     * @return void
	     */
	    void incr(Php::Parameters &params) {
	        if (params.size() < 1) {
	            throw Php::Exception("Requires parameter: key");
	            return;
	        }
	        int64_t iv = 1;
	        if (params.size() > 1) {
	            iv = params[1].numericValue();
	        }

	        this->status = db->Merge(rocksdb::WriteOptions(), params[0].stringValue(), std::to_string(iv));
	    }
	};
}











// Symbols are exported according to the "C" language
extern "C" 
{
    // export the "get_module" function that will be called by the Zend engine
    PHPCPP_EXPORT void *get_module()
    {
        // create extension
        static Php::Extension extension("RocksDBphp","0.1");


        Php::Class<RocksDBPHP::Driver> RocksDBPphpDriver("RocksDB");


		//RocksDBPphpDriver.method("myMethod", &TestBaseClass::MyCustomClass::myMethod, Php::Final, {});













        RocksDBPphpDriver.method("__construct", &RocksDBPHP::Driver::__construct, {
            Php::ByVal("path", Php::Type::String),
            Php::ByVal("cifm", Php::Type::Bool)
        }),

        // GET
        RocksDBPphpDriver.method("get", &RocksDBPHP::Driver::get, {
            Php::ByVal("key", Php::Type::String)
        }),
        RocksDBPphpDriver.method("__get", &RocksDBPHP::Driver::get, {
            Php::ByVal("key", Php::Type::String)
        }),
        RocksDBPphpDriver.method("mget", &RocksDBPHP::Driver::mget, {
            Php::ByVal("keys", Php::Type::Array)
        }),

        // DEL
        RocksDBPphpDriver.method("del", &RocksDBPHP::Driver::del, {
            Php::ByVal("key", Php::Type::String)
        }),
        RocksDBPphpDriver.method("__unset", &RocksDBPHP::Driver::del, {
            Php::ByVal("key", Php::Type::String)
        }),
        RocksDBPphpDriver.method("mdel", &RocksDBPHP::Driver::mdel, {
            Php::ByVal("keys", Php::Type::Array)
        }),

        // SET
        RocksDBPphpDriver.method("set", &RocksDBPHP::Driver::set, {
            Php::ByVal("key", Php::Type::String),
            Php::ByVal("val", Php::Type::String)
        }),
        RocksDBPphpDriver.method("__set", &RocksDBPHP::Driver::set, {
            Php::ByVal("key", Php::Type::String),
            Php::ByVal("val", Php::Type::String)
        }),
        RocksDBPphpDriver.method("mset", &RocksDBPHP::Driver::mset, {
            Php::ByVal("keys", Php::Type::Array),
            Php::ByVal("vals", Php::Type::Array)
        }),
        // OTHER
        RocksDBPphpDriver.method("getStatus", &RocksDBPHP::Driver::getStatus),

        RocksDBPphpDriver.method("incr", &RocksDBPHP::Driver::incr, {
            Php::ByVal("key", Php::Type::String),
            Php::ByVal("incrVal", Php::Type::Numeric)
        }),

        RocksDBPphpDriver.method("isset", &RocksDBPHP::Driver::isset, {
            Php::ByVal("key", Php::Type::String),
            //Php::ByRef("val", Php::Type::String) not work. required fix at PHP-CPP
        }),
        RocksDBPphpDriver.method("__isset", &RocksDBPHP::Driver::isset, {
            Php::ByVal("key", Php::Type::String)
        }),

















        
        // add the class to the extension
        extension.add(std::move(RocksDBPphpDriver));

        
        // return the extension module
        return extension;
    }
}