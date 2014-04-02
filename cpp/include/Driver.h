/**
 *  Driver.h
 *  The main class implements the functionality of the driver that will be exported in PHP
 *  
 *  @author valmat <ufabiz@gmail.com>
 *  @github https://github.com/valmat/rocksdbphp
 */



namespace RocksDBPHP {

	
	class Driver : public Php::Base//, public zend_object_iterator
	{
	private:
	    /**
	     * db
	     */
	    rocksdb::DB* _db;

	    /**
	     * db path distantion
	     */
	    std::string _dbpath;

	    /**
	     * db path distantion
	     */
	    rocksdb::Options _dboptions;

	    /**
	     * last opreation status
	     */
	    rocksdb::Status _status;
    


	public:

	    Driver() = default;// noexcept

	    /**
	     * function __construct
	     * @param string path
	     * @param bool create_if_missing
	     */
	    virtual void __construct(Php::Parameters &params)
	    {

	        if (params.size() < 1) {
	            throw Php::Exception("Requires parameter path");
	            return;
	        }
	        _dbpath = params[0].stringValue();
	        bool create_if_missing = true;
	        if (params.size() > 1) {
	            create_if_missing = (bool)params[1];
	        }

	        _dboptions.create_if_missing = create_if_missing;
	        _dboptions.merge_operator.reset(new Int64Incrementor);
	        _status = rocksdb::DB::Open(_dboptions, _dbpath, &_db);
	        //Php::out << _status.ToString() << std::endl;

	        if (!_status.ok())  {
	            throw Php::Exception( _status.ToString()  );
	        }
	    }

	    virtual ~Driver() {
	        //delete Incrementor; //not required : std::shared_ptr
	        delete _db;
	        Php::out << "~Driver()" << std::endl;
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
	        //Php::out << "set("<< key << ")=" << value << std::endl;

	        _status = _db->Put(rocksdb::WriteOptions(), key, value);
	        return (bool)(_status.ok());
	    }

	    /**
	     * Set multiple values ​​using an array or object
	     * function mset
	     * @param Php::Array keys
	     * @param Php::Array values
	     * @return bool rezult status
	     */
	    Php::Value mset(Php::Parameters &params)
	    {
	        // Check parameters
	        if (params.size() < 1) {
	            throw Php::Exception("Requires 1 parameter: array or itarable object");
	            return false;
	        }
	        if(!params[0].isArray() && !params[1].isObject() ) {
	            throw Php::Exception("Required parameters is array or itarable object");
	            return false;
	        }

	        // create RocksDB batch
	        rocksdb::WriteBatch batch;
	        // iterate over param to filling batch
	        for(auto &iter: params[0])
	        {
	            //Php::out << "set("<< iter.first.stringValue() << ")=" << iter.second.stringValue() << std::endl;
	            batch.Put(iter.first.stringValue(), iter.second.stringValue());
	        }
	        _status = _db->Write(rocksdb::WriteOptions(), &batch);
	        return (bool)(_status.ok());
	    }

	    /**
	     * get value by key
	     * function get
	     * @param string key
	     * @return string value or NULL (if the key is not exist)
	     */
	    Php::Value get(Php::Parameters &params)
	    {
	        if (params.size() < 1) {
	            throw Php::Exception("Requires 1 parameters: key");
	            return false;
	        }
	        std::string key = params[0].stringValue();
	        std::string value;

	        _status = _db->Get(rocksdb::ReadOptions(), key, &value);
	        if (!_status.ok()) {
	            return nullptr; // cast to `Php::NULL`
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
	            throw Php::Exception("Requires 1 parameter: array keys or itarable object");
	            return false;
	        }
	        if(!params[0].isArray()) {
	            throw Php::Exception("Required parameters is array or itarable object");
	            return false;
	        }

	        Php::Value rez;
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
	        statuses = _db->MultiGet(rocksdb::ReadOptions(), keys, &values);
	        for (unsigned int i = 0; i < arrSize; i++) {
	            rez[ks[i]] = (statuses[i].ok()) ? (Php::Value) values[i] : nullptr;
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

	        r = _db->KeyMayExist(rocksdb::ReadOptions(), key, &value);
	        //It is not guaranteed that value will be determined
	        //Php::out << "Get(" << key <<") = " << value << std::endl;
	        //Php::out << "sz" << sz << std::endl;

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

	        _status = _db->Delete(rocksdb::WriteOptions(), params[0].stringValue());
	        return (bool)(_status.ok());
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
	        _status = _db->Write(rocksdb::WriteOptions(), &batch);

	        return (bool)(_status.ok());
	    }

	    /**
	     * function getStatus
	     * @param void
	     * @return string status.ToString()
	     */
	    Php::Value getStatus() {
	        return  _status.ToString();
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

	        _status = _db->Merge(rocksdb::WriteOptions(), params[0].stringValue(), std::to_string(iv));
	    }
	};
}
