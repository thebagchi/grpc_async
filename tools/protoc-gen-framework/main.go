package main

import (
	"strings"

	"google.golang.org/protobuf/compiler/protogen"
)

func main() {
	protogen.Options{}.Run(func(gen *protogen.Plugin) error {
		for _, f := range gen.Files {
			if !f.Generate || len(f.Services) == 0 {
				continue
			}
			if err := genrate(gen, f); err != nil {
				return err
			}
		}
		return nil
	})
}

func genrate(gen *protogen.Plugin, f *protogen.File) error {
	path := f.Desc.Path()
	base := strings.TrimSuffix(path, EXT_PROTO)
	if idx := strings.LastIndex(base, "/"); idx >= 0 {
		base = base[idx+1:]
	}
	name := base + EXT_FRAMEWORK

	g := gen.NewGeneratedFile(name, f.GoImportPath)

	pkg := string(f.Desc.Package())
	namespace := strings.ReplaceAll(pkg, ".", "::")

	guard := strings.ToUpper(
		strings.NewReplacer("/", "_", ".", "_", "-", "_").Replace(name),
	) + GUARD_SUFFIX

	model := File{
		Guard:     guard,
		Namespace: namespace,
		Includes: []string{
			base + EXT_GRPC_STUB,
		},
	}

	for _, s := range f.Services {
		svc := &Service{
			Name: string(s.Desc.Name()),
		}
		for _, m := range s.Methods {
			svc.Methods = append(svc.Methods, &Method{
				Name:         string(m.Desc.Name()),
				Input:        MakeType(m.Input),
				Output:       MakeType(m.Output),
				ClientStream: m.Desc.IsStreamingClient(),
				ServerStream: m.Desc.IsStreamingServer(),
			})
		}
		model.Services = append(model.Services, svc)
	}

	return MakeHeader(g, model)
}

// MakeType converts a proto fully-qualified message name to its C++ equivalent.
func MakeType(m *protogen.Message) string {
	return "::" + strings.ReplaceAll(string(m.Desc.FullName()), ".", "::")
}
