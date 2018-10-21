open MongoUtils;

exception MongoAdmin_failed(string);

type t = Mongo_lwt.t;

type cmd = {
  name: string,
  query: string,
};

let admin_db_name = "admin";
let admin_collection_name = "$cmd";

let get_db_name = Mongo_lwt.get_db_name;
let get_collection_name = Mongo_lwt.get_collection_name;
let get_ip = Mongo_lwt.get_ip;
let get_port = Mongo_lwt.get_port;
let get_channel_pool = Mongo_lwt.get_channel_pool;
/* let get_channels = Mongo_lwt.get_channels;; */
/* let get_output_channel = Mongo_lwt.get_output_channel;; */
/* let get_input_channel = Mongo_lwt.get_input_channel;; */

let wrap_bson = (f, arg) =>
  try (f(arg)) {
  | ObjectId.InvalidObjectId =>
    raise(MongoAdmin_failed("ObjectId.InvalidObjectId"))
  | Bson.WrongBsonType =>
    raise(MongoAdmin_failed("wrongBsonType when encoding bson doc"))
  | Bson.Malformed_bson =>
    raise(MongoAdmin_failed("Malformed_bson when decoding bson"))
  };

let wrap_unix_lwt = (f, arg) =>
  try%lwt (f(arg)) {
  | [@implicit_arity] Unix.Unix_error(e, _, _) =>
    raise(MongoAdmin_failed(Unix.error_message(e)))
  };

let create = (~max_connection=?, ip, port) =>
  Mongo_lwt.create(
    ~max_connection?,
    ip,
    port,
    admin_db_name,
    admin_collection_name,
  );
let createLocalDefault = () => create("127.0.0.1", 27017);

let destroy = a => Mongo_lwt.destroy(a);

let get_request_id = cur_timestamp;

let create_cmd = name => {
  let e_1 = Bson.createInt32(1l);
  let cmd_doc = name => Bson.addElement(name, e_1, Bson.empty);
  {
    name,
    query: {
      let find_in = ((flags, skip, return, q, s)) =>
        MongoRequest.create_query(
          (admin_db_name, admin_collection_name),
          (get_request_id(), flags, skip, return),
          (q, s),
        );
      wrap_bson(find_in, (0l, 0l, (-1l), cmd_doc(name), Bson.empty));
    },
  };
};

let send_cmd = ((a, cmd)) =>
  MongoSend_lwt.send_with_reply(Mongo_lwt.get_channel_pool(a), cmd.query);

let listDatabases = a =>
  wrap_unix_lwt(send_cmd, (a, create_cmd("listDatabases")));
let buildInfo = a => wrap_unix_lwt(send_cmd, (a, create_cmd("buildInfo")));
let collStats = a => wrap_unix_lwt(send_cmd, (a, create_cmd("collStats")));
let connPoolStats = a =>
  wrap_unix_lwt(send_cmd, (a, create_cmd("connPoolStats")));
let cursorInfo = a =>
  wrap_unix_lwt(send_cmd, (a, create_cmd("cursorInfo")));
let getCmdLineOpts = a =>
  wrap_unix_lwt(send_cmd, (a, create_cmd("getCmdLineOpts")));
let hostInfo = a => wrap_unix_lwt(send_cmd, (a, create_cmd("hostInfo")));
let listCommands = a =>
  wrap_unix_lwt(send_cmd, (a, create_cmd("listCommands")));
let serverStatus = a =>
  wrap_unix_lwt(send_cmd, (a, create_cmd("serverStatus")));