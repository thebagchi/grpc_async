package main

// ── File extensions ───────────────────────────────────────────────────────────

const (
	EXT_PROTO     = ".proto"
	EXT_FRAMEWORK = ".framework.pb.h"
	EXT_GRPC_STUB = ".grpc.pb.h"
	GUARD_SUFFIX  = "_INCLUDED"
)

// ── Framework server-side call classes ───────────────────────────────────────

const (
	CALL_UNARY         = "framework::UnaryCall"
	CALL_SERVER_STREAM = "framework::ServerStreamCall"
	CALL_CLIENT_STREAM = "framework::ClientStreamCall"
	CALL_BIDI          = "framework::BidiCall"
)

// ── Framework client-side stream classes ─────────────────────────────────────

const (
	STREAM_UNARY    = "framework::UnaryStream"
	STREAM_INBOUND  = "framework::InboundStream"
	STREAM_OUTBOUND = "framework::OutboundStream"
	STREAM_DUPLEX   = "framework::DuplexStream"
)

// ── RPC shape comments ────────────────────────────────────────────────────────

const (
	COMMENT_UNARY         = "unary — callback receives (Ptr<response>, Status) once."
	COMMENT_SERVER_STREAM = "server-streaming — E_DATA per message, then E_FINISHED."
	COMMENT_CLIENT_STREAM = "client-streaming — E_READY (Write... then Close), then E_FINISHED."
	COMMENT_BIDI          = "bidirectional — E_READY, E_DATA, then E_FINISHED."
)
