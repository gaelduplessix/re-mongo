open MongoUtils;

exception Mongo_failed(string);

type t = {
  db_name: string,
  collection_name: string,
  ip: string,
  port: int,
  file_descr: Unix.file_descr,
};

let get_db_name = m => m.db_name;
let get_collection_name = m => m.collection_name;
let get_ip = m => m.ip;
let get_port = m => m.port;
let get_file_descr = m => m.file_descr;
let change_collection = (m, c) => {...m, collection_name: c};

let wrap_bson = (f, arg) =>
  try (f(arg)) {
  | ObjectId.InvalidObjectId =>
    raise(Mongo_failed("ObjectId.InvalidObjectId"))
  | Bson.WrongBsonType =>
    raise(Mongo_failed("wrongBsonType when encoding bson doc"))
  | Bson.Malformed_bson =>
    raise(Mongo_failed("Malformed_bson when decoding bson"))
  };

let wrap_unix = (f, arg) =>
  try (f(arg)) {
  | [@implicit_arity] Unix.Unix_error(e, _, _) =>
    raise(Mongo_failed(Unix.error_message(e)))
  };

let connect_to = ((ip, port)) => {
  let c_descr = Unix.socket(Unix.PF_INET, Unix.SOCK_STREAM, 0);
  let s_addr =
    [@implicit_arity] Unix.ADDR_INET(Unix.inet_addr_of_string(ip), port);
  Unix.connect(c_descr, s_addr);
  c_descr;
};

let create = (ip, port, db_name, collection_name) => {
  db_name,
  collection_name,
  ip,
  port,
  file_descr: wrap_unix(connect_to, (ip, port)),
};

let createLocalDefault = (db_name, collection_name) =>
  create("127.0.0.1", 27017, db_name, collection_name);

let destroy = m => wrap_unix(Unix.close, m.file_descr);

let get_request_id = cur_timestamp;

let send_only = ((m, str)) => MongoSend.send_no_reply(m.file_descr, str);
let send = ((m, str)) => MongoSend.send_with_reply(m.file_descr, str);

let insert_in = ((m, flags, doc_list)) =>
  MongoRequest.create_insert(
    (m.db_name, m.collection_name),
    (get_request_id(), flags),
    doc_list,
  );
let insert = (m, doc_list) =>
  wrap_unix(send_only, (m, wrap_bson(insert_in, (m, 0l, doc_list))));

let update_in = ((m, flags, s, u)) =>
  MongoRequest.create_update(
    (m.db_name, m.collection_name),
    (get_request_id(), flags),
    (s, u),
  );
let update_one = (~upsert=false, m, (s, u)) =>
  wrap_unix(
    send_only,
    (m, wrap_bson(update_in, (m, if (upsert) {1l} else {0l}, s, u))),
  );
let update_all = (~upsert=false, m, (s, u)) =>
  wrap_unix(
    send_only,
    (m, wrap_bson(update_in, (m, if (upsert) {3l} else {2l}, s, u))),
  );

let delete_in = ((m, flags, s)) =>
  MongoRequest.create_delete(
    (m.db_name, m.collection_name),
    (get_request_id(), flags),
    s,
  );
let delete_one = (m, s) =>
  wrap_unix(send_only, (m, wrap_bson(delete_in, (m, 1l, s))));
let delete_all = (m, s) =>
  wrap_unix(send_only, (m, wrap_bson(delete_in, (m, 0l, s))));

let find_in = ((m, flags, skip, return, q, s)) =>
  MongoRequest.create_query(
    (m.db_name, m.collection_name),
    (get_request_id(), flags, skip, return),
    (q, s),
  );
let find = (~skip=0, m) =>
  wrap_unix(
    send,
    (
      m,
      wrap_bson(
        find_in,
        (m, 0l, Int32.of_int(skip), 0l, Bson.empty, Bson.empty),
      ),
    ),
  );
let find_one = (~skip=0, m) =>
  wrap_unix(
    send,
    (
      m,
      wrap_bson(
        find_in,
        (m, 0l, Int32.of_int(skip), 1l, Bson.empty, Bson.empty),
      ),
    ),
  );
let find_of_num = (~skip=0, m, num) =>
  wrap_unix(
    send,
    (
      m,
      wrap_bson(
        find_in,
        (
          m,
          0l,
          Int32.of_int(skip),
          Int32.of_int(num),
          Bson.empty,
          Bson.empty,
        ),
      ),
    ),
  );
let find_q = (~skip=0, m, q) =>
  wrap_unix(
    send,
    (
      m,
      wrap_bson(find_in, (m, 0l, Int32.of_int(skip), 0l, q, Bson.empty)),
    ),
  );
let find_q_one = (~skip=0, m, q) =>
  wrap_unix(
    send,
    (
      m,
      wrap_bson(find_in, (m, 0l, Int32.of_int(skip), 1l, q, Bson.empty)),
    ),
  );
let find_q_of_num = (~skip=0, m, q, num) =>
  wrap_unix(
    send,
    (
      m,
      wrap_bson(
        find_in,
        (m, 0l, Int32.of_int(skip), Int32.of_int(num), q, Bson.empty),
      ),
    ),
  );
let find_q_s = (~skip=0, m, q, s) =>
  wrap_unix(
    send,
    (m, wrap_bson(find_in, (m, 0l, Int32.of_int(skip), 0l, q, s))),
  );
let find_q_s_one = (~skip=0, m, q, s) =>
  wrap_unix(
    send,
    (m, wrap_bson(find_in, (m, 0l, Int32.of_int(skip), 1l, q, s))),
  );
let find_q_s_of_num = (~skip=0, m, q, s, num) =>
  wrap_unix(
    send,
    (
      m,
      wrap_bson(
        find_in,
        (m, 0l, Int32.of_int(skip), Int32.of_int(num), q, s),
      ),
    ),
  );

let count = (~skip=?, ~limit=?, ~query=Bson.empty, m) => {
  let c_bson =
    Bson.addElement("query", Bson.createDocElement(query), Bson.empty);
  let c_bson =
    Bson.addElement("count", Bson.createString(m.collection_name), c_bson);
  let c_bson =
    switch (limit) {
    | Some(n) =>
      Bson.addElement("limit", Bson.createInt32(Int32.of_int(n)), c_bson)
    | None => c_bson
    };

  let c_bson =
    switch (skip) {
    | Some(n) =>
      Bson.addElement("skip", Bson.createInt32(Int32.of_int(n)), c_bson)
    | None => c_bson
    };

  let m = change_collection(m, "$cmd");
  let r = find_q_one(m, c_bson);
  let d = List.nth(MongoReply.getDocumentList(r), 0);
  int_of_float(Bson.getDouble(Bson.getElement("n", d)));
};

let get_more_in = ((m, c, num)) =>
  MongoRequest.create_get_more(
    (m.db_name, m.collection_name),
    (get_request_id(), Int32.of_int(num)),
    c,
  );
let get_more_of_num = (m, c, num) =>
  wrap_unix(send, (m, wrap_bson(get_more_in, (m, c, num))));
let get_more = (m, c) => get_more_of_num(m, c, 0);

let kill_cursors_in = c_list =>
  MongoRequest.create_kill_cursors(get_request_id(), c_list);
let kill_cursors = (m, c_list) =>
  wrap_unix(send_only, (m, wrap_bson(kill_cursors_in, c_list)));

let drop_database = m => {
  let m = change_collection(m, "$cmd");
  find_q_one(
    m,
    Bson.addElement("dropDatabase", Bson.createInt32(1l), Bson.empty),
  );
};

let drop_collection = m => {
  let m_ = change_collection(m, "$cmd");
  find_q_one(
    m_,
    Bson.addElement(
      "drop",
      Bson.createString(m.collection_name),
      Bson.empty,
    ),
  );
};

/** INDEX **/

let get_indexes = m => {
  let m_ = change_collection(m, "system.indexes");
  find_q(
    m_,
    Bson.addElement(
      "ns",
      Bson.createString(m.db_name ++ "." ++ m.collection_name),
      Bson.empty,
    ),
  );
};

type index_option =
  | Background(bool)
  | Unique(bool)
  | Name(string)
  | DropDups(bool)
  | Sparse(bool)
  | ExpireAfterSeconds(int)
  | V(int)
  | Weight(Bson.t)
  | Default_language(string)
  | Language_override(string);

let ensure_index = (m, key_bson, options) => {
  let default_name = () => {
    let doc = Bson.getElement("key", key_bson);

    List.fold_left(
      (s, (k, e)) => {
        let i = Bson.getInt32(e);
        if (s == "") {
          Printf.sprintf("%s_%ld", k, i);
        } else {
          Printf.sprintf("%s_%s_%ld", s, k, i);
        };
      },
      "",
      Bson.allElements(Bson.getDocElement(doc)),
    );
  };

  let has_name = ref(false);
  let has_version = ref(false);
  /* check all options */

  let main_bson =
    List.fold_left(
      (acc, o) =>
        switch (o) {
        | Background(b) =>
          Bson.addElement("background", Bson.createBoolean(b), acc)
        | Unique(b) => Bson.addElement("unique", Bson.createBoolean(b), acc)
        | Name(s) =>
          has_name := true;
          Bson.addElement("name", Bson.createString(s), acc);
        | DropDups(b) =>
          Bson.addElement("dropDups", Bson.createBoolean(b), acc)
        | Sparse(b) => Bson.addElement("sparse", Bson.createBoolean(b), acc)
        | ExpireAfterSeconds(i) =>
          Bson.addElement(
            "expireAfterSeconds",
            Bson.createInt32(Int32.of_int(i)),
            acc,
          )
        | V(i) =>
          if (i != 0 && i != 1) {
            raise(Mongo_failed("Version number for index must be 0 or 1"));
          };
          has_version := true;
          Bson.addElement("v", Bson.createInt32(Int32.of_int(i)), acc);
        | Weight(bson) =>
          Bson.addElement("weights", Bson.createDocElement(bson), acc)
        | Default_language(s) =>
          Bson.addElement("default_language", Bson.createString(s), acc)
        | Language_override(s) =>
          Bson.addElement("language_override", Bson.createString(s), acc)
        },
      key_bson,
      options,
    );

  /* check if then name has been set, create a default name otherwise */
  let main_bson =
    if (has_name^ == false) {
      Bson.addElement("name", Bson.createString(default_name()), main_bson);
    } else {
      main_bson;
    };

  /* check if the version has been set, set 1 otherwise */
  let main_bson =
    if (has_version^ == false) {
      Bson.addElement("v", Bson.createInt32(1l), main_bson);
    } else {
      main_bson;
    };

  let main_bson =
    Bson.addElement(
      "ns",
      Bson.createString(m.db_name ++ "." ++ m.collection_name),
      main_bson,
    );

  let system_indexes_m = change_collection(m, "system.indexes");

  insert(system_indexes_m, [main_bson]);
};

let ensure_simple_index = (~options=[], m, field) => {
  let key_bson =
    Bson.addElement(
      "key",
      Bson.createDocElement(
        Bson.addElement(field, Bson.createInt32(1l), Bson.empty),
      ),
      Bson.empty,
    );
  ensure_index(m, key_bson, options);
};

let ensure_multi_simple_index = (~options=[], m, fields) => {
  let key_bson =
    List.fold_left(
      (acc, f) => Bson.addElement(f, Bson.createInt32(1l), acc),
      Bson.empty,
      fields,
    );

  let key_bson =
    Bson.addElement("key", Bson.createDocElement(key_bson), Bson.empty);
  ensure_index(m, key_bson, options);
};

let drop_index = (m, index_name) => {
  let index_bson =
    Bson.addElement("index", Bson.createString(index_name), Bson.empty);
  let delete_bson =
    Bson.addElement(
      "deleteIndexes",
      Bson.createString(m.collection_name),
      index_bson,
    );
  let m = change_collection(m, "$cmd");
  find_q_one(m, delete_bson);
};

let drop_all_index = m => drop_index(m, "*");