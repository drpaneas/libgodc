// godc - Dreamcast Go CLI
package main

import (
	"bufio"
	"bytes"
	_ "embed"
	"fmt"
	"io"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"text/template"
	"time"

	"github.com/BurntSushi/toml"
)

//go:embed templates/Makefile.tmpl
var mkTmpl string

//go:embed templates/gitignore.tmpl
var giTmpl string

//go:embed templates/go.mod.tmpl
var modTmpl string

const tcVer = "gcc15.1.0-kos2.2.1"
const tcURL = "https://github.com/drpaneas/dreamcast-toolchain-builds/releases/download/" + tcVer
const repo = "https://github.com/drpaneas/libgodc.git"

var tcFiles = map[string]string{
	"darwin/arm64": "dreamcast-toolchain-" + tcVer + "-darwin-arm64.tar.gz",
	"darwin/amd64": "dreamcast-toolchain-" + tcVer + "-darwin-x86_64.tar.gz",
	"linux/amd64":  "dreamcast-toolchain-" + tcVer + "-linux-x86_64.tar.gz",
}

type cfg struct{ Path, Emu, IP string }

func (c *cfg) kos() string { return c.Path + "/kos" }

func cfgPath() string { h, _ := os.UserHomeDir(); return h + "/.config/godc/config.toml" }

func load() *cfg {
	var c cfg
	toml.DecodeFile(cfgPath(), &c)
	if c.Path == "" {
		// Try KOS_BASE env first, then default to ~/dreamcast
		if kos := os.Getenv("KOS_BASE"); kos != "" {
			c.Path = filepath.Dir(kos) // /opt/toolchains/dc/kos -> /opt/toolchains/dc
		} else {
			h, _ := os.UserHomeDir()
			c.Path = h + "/dreamcast"
		}
		c.IP = "192.168.2.203"
		c.Emu = "flycast"
		if runtime.GOOS == "darwin" {
			c.Emu = "/Applications/Flycast.app/Contents/MacOS/Flycast"
		}
	}
	return &c
}

func (c *cfg) save() {
	os.MkdirAll(filepath.Dir(cfgPath()), 0755)
	f, _ := os.Create(cfgPath())
	toml.NewEncoder(f).Encode(c)
	f.Close()
}

func env(c *cfg) []string {
	k := c.kos()
	m := map[string]string{
		"KOS_BASE": k, "KOS_CC_BASE": c.Path + "/sh-elf", "KOS_PORTS": c.Path + "/kos-ports",
		"KOS_ARCH": "dreamcast", "KOS_SUBARCH": "pristine",
		"PATH": c.Path + "/sh-elf/bin:" + k + "/utils/build_wrappers:" + os.Getenv("PATH"),
	}
	for _, v := range os.Environ() {
		if i := strings.IndexByte(v, '='); i > 0 {
			if _, ok := m[v[:i]]; !ok {
				m[v[:i]] = v[i+1:]
			}
		}
	}
	r := make([]string, 0, len(m))
	for k, v := range m {
		r = append(r, k+"="+v)
	}
	return r
}

func sh(name string, args []string, dir string, e []string) error {
	cmd := exec.Command(name, args...)
	cmd.Stdout, cmd.Stderr, cmd.Dir, cmd.Env = os.Stdout, os.Stderr, dir, e
	return cmd.Run()
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("godc: setup|config|init|build|run|doctor|env|update|version")
		return
	}
	c := load()
	var err error
	switch os.Args[1] {
	case "setup":
		err = setup(c)
	case "config":
		r := bufio.NewReader(os.Stdin)
		rd := func(p, d string) string {
			fmt.Printf("%s [%s]: ", p, d)
			if l, _ := r.ReadString('\n'); strings.TrimSpace(l) != "" {
				s := strings.TrimSpace(l)
				if strings.HasPrefix(s, "~/") {
					h, _ := os.UserHomeDir()
					return h + s[1:]
				}
				return s
			}
			return d
		}
		c.Path, c.Emu, c.IP = rd("Path", c.Path), rd("Emu", c.Emu), rd("IP", c.IP)
		c.save()
	case "init":
		cwd, _ := os.Getwd()
		name := filepath.Base(cwd)
		for _, t := range []struct{ f, c string }{
			{".Makefile", exec_(mkTmpl, name)},
			{".gitignore", giTmpl},
			{"go.mod", exec_(modTmpl, name)},
		} {
			// Always overwrite go.mod, only create other files if they don't exist
			if t.f == "go.mod" {
				os.WriteFile(t.f, []byte(t.c), 0644)
			} else if _, e := os.Stat(t.f); e != nil {
				os.WriteFile(t.f, []byte(t.c), 0644)
			}
		}
	case "build":
		var out string
		for i, a := range os.Args[2:] {
			if a == "-o" && i+1 < len(os.Args[2:]) {
				out = os.Args[i+3]
			}
		}
		cwd, _ := os.Getwd()
		if _, e := os.Stat(cwd + "/.Makefile"); e != nil {
			os.Args = []string{"", "init"}
			main()
		}
		args := []string{"-f", ".Makefile"}
		if out != "" {
			args = append(args, "OUTPUT="+out)
		}
		err = sh("make", args, "", env(c))
	case "run":
		ip := false
		for _, a := range os.Args[2:] {
			if a == "--ip" {
				ip = true
			}
		}
		tmp, _ := os.MkdirTemp("", "godc-*")
		defer os.RemoveAll(tmp)
		elf := tmp + "/game.elf"
		os.Args = []string{"", "build", "-o", elf}
		main()
		if ip {
			err = sh("dc-tool-ip", []string{"-t", c.IP, "-x", elf}, "", nil)
		} else {
			err = sh(c.Emu, []string{elf}, "", nil)
		}
	case "doctor":
		for _, p := range []struct{ n, p string }{
			{"kos", c.kos()}, {"libgodc", c.kos() + "/lib/libgodc.a"},
		} {
			m := "✗"
			if _, e := os.Stat(p.p); e == nil {
				m = "✓"
			}
			fmt.Println(m, p.n)
		}
	case "env":
		fmt.Printf("PATH=%s\nKOS=%s\n", c.Path, c.kos())
	case "update":
		lib := c.Path + "/libgodc"
		e := env(c)
		if _, err := os.Stat(lib); err != nil {
			sh("git", []string{"clone", repo, lib}, "", nil)
		} else {
			sh("git", []string{"-C", lib, "pull"}, "", nil)
		}
		sh("make", []string{"-C", lib, "clean"}, "", e)
		sh("make", []string{"-C", lib}, "", e)
		err = sh("make", []string{"-C", lib, "install"}, "", e)
	case "version":
		fmt.Println("godc 0.1.0")
	}
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func setup(c *cfg) error {
	h, _ := os.UserHomeDir()
	p := h + "/dreamcast"
	if e, _ := os.ReadDir(p); len(e) > 0 {
		return fmt.Errorf("%s not empty", p)
	}
	f := tcFiles[runtime.GOOS+"/"+runtime.GOARCH]
	if f == "" {
		return fmt.Errorf("unsupported platform")
	}
	tmp := os.TempDir() + "/" + f
	defer os.Remove(tmp)

	fmt.Println("Downloading...")
	resp, err := (&http.Client{Timeout: 30 * time.Minute}).Get(tcURL + "/" + f)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	out, _ := os.Create(tmp)
	n, _ := io.Copy(out, resp.Body)
	out.Close()
	fmt.Printf("%dMB\n", n/1024/1024)

	fmt.Println("Extracting...")
	os.MkdirAll(p, 0755)
	args := []string{"xzf", tmp, "-C", p}
	if o, _ := exec.Command("tar", "tzf", tmp).Output(); !strings.Contains(string(o), "\nkos/") {
		args = append(args, "--strip-components=1")
	}
	sh("tar", args, "", nil)

	c.Path = p
	c.save()

	lib := p + "/libgodc"
	fmt.Println("Cloning...")
	sh("git", []string{"clone", repo, lib}, "", nil)
	e := env(c)
	fmt.Println("Building...")
	sh("make", nil, lib, e)
	sh("make", []string{"install"}, lib, e)

	rc := h + "/.zshrc"
	if strings.Contains(os.Getenv("SHELL"), "bash") {
		rc = h + "/.bashrc"
	}
	if d, _ := os.ReadFile(rc); !strings.Contains(string(d), "godc") {
		f, _ := os.OpenFile(rc, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
		fmt.Fprintf(f, "\nexport PATH=\"%s/sh-elf/bin:$PATH\"\nsource %s/kos/environ.sh\n", p, p)
		f.Close()
	}
	fmt.Println("✓ Done")
	return nil
}

func exec_(t, name string) string {
	var b bytes.Buffer
	template.Must(template.New("").Parse(t)).Execute(&b, map[string]string{"Name": name, "Module": name})
	return b.String()
}
