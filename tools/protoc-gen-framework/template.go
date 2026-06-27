package main

import (
	_ "embed"
	"fmt"
	"io"
	"text/template"
)

//go:embed header.tmpl
var TEMPLATE_TEXT string

// MakeHeader executes the C++ header template into w.
func MakeHeader(w io.Writer, model File) error {
	return headerTemplate.Execute(w, model)
}

var headerTemplate = template.Must(
	template.New("framework_header").Funcs(template.FuncMap{
		"serverCall":   MakeServerCallClass,
		"clientStream": MakeClientStreamClass,
		"requestArg":   MakeRequestArg,
		"requestPass":  MakeRequestParam,
		"shapeComment": MakeShapeComment,
	}).Parse(TEMPLATE_TEXT),
)

// ── Data model ────────────────────────────────────────────────────────────────

type Method struct {
	Name         string
	Input        string
	Output       string
	ClientStream bool
	ServerStream bool
}

type Service struct {
	Name    string
	Methods []*Method
}

type File struct {
	Guard     string
	Namespace string
	Includes  []string
	Services  []*Service
}

// ── Template helpers ──────────────────────────────────────────────────────────

func MakeServerCallClass(m *Method) string {
	switch {
	case !m.ClientStream && !m.ServerStream:
		return CALL_UNARY
	case !m.ClientStream && m.ServerStream:
		return CALL_SERVER_STREAM
	case m.ClientStream && !m.ServerStream:
		return CALL_CLIENT_STREAM
	default:
		return CALL_BIDI
	}
}

func MakeClientStreamClass(m *Method) string {
	switch {
	case !m.ClientStream && !m.ServerStream:
		return STREAM_UNARY
	case !m.ClientStream && m.ServerStream:
		return STREAM_INBOUND
	case m.ClientStream && !m.ServerStream:
		return STREAM_OUTBOUND
	default:
		return STREAM_DUPLEX
	}
}

func MakeRequestArg(m *Method) string {
	if m.ClientStream {
		return ""
	}
	return fmt.Sprintf("const %s& request, ", m.Input)
}

func MakeRequestParam(m *Method) string {
	if m.ClientStream {
		return ""
	}
	return "request, "
}

func MakeShapeComment(m *Method) string {
	switch {
	case !m.ClientStream && !m.ServerStream:
		return COMMENT_UNARY
	case !m.ClientStream && m.ServerStream:
		return COMMENT_SERVER_STREAM
	case m.ClientStream && !m.ServerStream:
		return COMMENT_CLIENT_STREAM
	default:
		return COMMENT_BIDI
	}
}
