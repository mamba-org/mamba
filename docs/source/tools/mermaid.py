"""
sphinx-mermaid
~~~~~~~~~~~~~~~

https://github.com/mgaitan/sphinxcontrib-mermaid
Modified for the purpose of CoSApp by the CoSApp team
https://gitlab.com/cosapp/cosapp

Allow mermaid diagramas to be included in Sphinx-generated
documents inline.

:copyright: Copyright 2016 by Martín Gaitán and others, see AUTHORS.
:license: BSD, see LICENSE for details.
"""

import codecs
import os
import posixpath
from errno import ENOENT
from hashlib import sha1
from subprocess import PIPE, Popen
from tempfile import _get_default_tempdir

import sphinx
from docutils import nodes
from docutils.parsers.rst import Directive, directives
from docutils.statemachine import ViewList
from six import text_type
from sphinx.application import Sphinx
from sphinx.errors import SphinxError
from sphinx.locale import _
from sphinx.util import logging
from sphinx.util.fileutil import copy_asset
from sphinx.util.i18n import search_image_for_language
from sphinx.util.osutil import ensuredir

logger = logging.getLogger(__name__)


class MermaidError(SphinxError):
    category = "Mermaid error"


class mermaid(nodes.General, nodes.Inline, nodes.Element):
    pass


def figure_wrapper(directive, node, caption):
    figure_node = nodes.figure("", node)
    if "align" in node:
        figure_node["align"] = node.attributes.pop("align")

    parsed = nodes.Element()
    directive.state.nested_parse(ViewList([caption], source=""), directive.content_offset, parsed)
    caption_node = nodes.caption(parsed[0].rawsource, "", *parsed[0].children)
    caption_node.source = parsed[0].source
    caption_node.line = parsed[0].line
    figure_node += caption_node
    return figure_node


def align_spec(argument):
    return directives.choice(argument, ("left", "center", "right"))


class Mermaid(Directive):
    """
    Directive to insert arbitrary Mermaid markup.
    """

    has_content = True
    required_arguments = 0
    optional_arguments = 1
    final_argument_whitespace = False
    option_spec = {
        "alt": directives.unchanged,
        "align": align_spec,
        "caption": directives.unchanged,
    }

    def get_mm_code(self):
        if self.arguments:
            # try to load mermaid code from an external file
            document = self.state.document
            if self.content:
                return [
                    document.reporter.warning(
                        "Mermaid directive cannot have both content and " "a filename argument",
                        line=self.lineno,
                    )
                ]
            env = self.state.document.settings.env
            argument = search_image_for_language(self.arguments[0], env)
            rel_filename, filename = env.relfn2path(argument)
            env.note_dependency(rel_filename)
            try:
                with codecs.open(filename, "r", "utf-8") as fp:
                    mmcode = fp.read()
            except (IOError, OSError):  # noqa
                return [
                    document.reporter.warning(
                        "External Mermaid file %r not found or reading " "it failed" % filename,
                        line=self.lineno,
                    )
                ]
        else:
            # inline mermaid code
            mmcode = "\n".join(self.content)
            if not mmcode.strip():
                return [
                    self.state_machine.reporter.warning(
                        'Ignoring "mermaid" directive without content.',
                        line=self.lineno,
                    )
                ]
        return mmcode

    def run(self):
        node = mermaid()
        node["code"] = self.get_mm_code()
        node["options"] = {}
        if "alt" in self.options:
            node["alt"] = self.options["alt"]
        if "align" in self.options:
            node["align"] = self.options["align"]
        if "inline" in self.options:
            node["inline"] = True

        caption = self.options.get("caption")
        if caption:
            node = figure_wrapper(self, node, caption)

        return [node]


def render_mm(self, code, options, fmt, prefix="mermaid"):
    """Render mermaid code into a PNG or PDF output file."""

    if fmt == "raw":
        fmt = "png"

    mermaid_cmd = self.builder.config.mermaid_cmd
    hashkey = (code + str(options) + str(self.builder.config.mermaid_sequence_config)).encode(
        "utf-8"
    )

    basename = "%s-%s" % (prefix, sha1(hashkey).hexdigest())
    fname = "%s.%s" % (basename, fmt)
    relfn = posixpath.join(self.builder.imgpath, fname)
    outdir = os.path.join(self.builder.outdir, self.builder.imagedir)
    outfn = os.path.join(outdir, fname)
    tmpfn = os.path.join(_get_default_tempdir(), basename)

    if os.path.isfile(outfn):
        return relfn, outfn

    ensuredir(os.path.dirname(outfn))

    # mermaid expects UTF-8 by default
    if isinstance(code, text_type):
        code = code.encode("utf-8")

    with open(tmpfn, "wb") as t:
        t.write(code)

    mm_args = [mermaid_cmd, "-i", tmpfn, "-o", outfn]
    mm_args.extend(self.builder.config.mermaid_params)
    if self.builder.config.mermaid_sequence_config:
        mm_args.extend("--configFile", self.builder.config.mermaid_sequence_config)

    try:
        p = Popen(mm_args, stdout=PIPE, stdin=PIPE, stderr=PIPE)
    except OSError as err:
        if err.errno != ENOENT:  # No such file or directory
            raise
        logger.warning(
            "command %r cannot be run (needed for mermaid "
            "output), check the mermaid_cmd setting" % mermaid_cmd
        )
        return None, None

    stdout, stderr = p.communicate(code)
    if self.builder.config.mermaid_verbose:
        logger.info(stdout)

    if p.returncode != 0:
        raise MermaidError(
            "Mermaid exited with error:\n[stderr]\n%s\n" "[stdout]\n%s" % (stderr, stdout)
        )
    if not os.path.isfile(outfn):
        raise MermaidError(
            "Mermaid did not produce an output file:\n[stderr]\n%s\n"
            "[stdout]\n%s" % (stderr, stdout)
        )
    return relfn, outfn


def _render_mm_html_raw(self, node, code, options, prefix="mermaid", imgcls=None, alt=None):
    if "align" in node:
        tag_template = """<div align="{align}" class="mermaid align-{align}">
            {code}
        </div>
        """
    else:
        tag_template = """<div class="mermaid">
            {code}
        </div>"""

    self.body.append(tag_template.format(align=node.get("align"), code=self.encode(code)))
    raise nodes.SkipNode


def render_mm_html(self, node, code, options, prefix="mermaid", imgcls=None, alt=None):
    fmt = self.builder.config.mermaid_output_format
    if fmt == "raw":
        return _render_mm_html_raw(
            self, node, code, options, prefix="mermaid", imgcls=None, alt=None
        )

    try:
        if fmt not in ("png", "svg"):
            raise MermaidError(
                "mermaid_output_format must be one of 'raw', 'png', " "'svg', but is %r" % fmt
            )

        fname, outfn = render_mm(self, code, options, fmt, prefix)
    except MermaidError as exc:
        logger.warning("mermaid code %r: " % code + str(exc))
        raise nodes.SkipNode

    if fname is None:
        self.body.append(self.encode(code))
    else:
        if alt is None:
            alt = node.get("alt", self.encode(code).strip())
        imgcss = imgcls and 'class="%s"' % imgcls or ""
        if fmt == "svg":
            svgtag = """<object data="%s" type="image/svg+xml">
            <p class="warning">%s</p></object>\n""" % (
                fname,
                alt,
            )
            self.body.append(svgtag)
        else:
            if "align" in node:
                self.body.append(
                    '<div align="%s" class="align-%s">' % (node["align"], node["align"])
                )

            self.body.append('<img src="%s" alt="%s" %s/>\n' % (fname, alt, imgcss))
            if "align" in node:
                self.body.append("</div>\n")

    raise nodes.SkipNode


def html_visit_mermaid(self, node):
    render_mm_html(self, node, node["code"], node["options"])


def render_mm_latex(self, node, code, options, prefix="mermaid"):
    try:
        fname, outfn = render_mm(self, code, options, "pdf", prefix)
    except MermaidError as exc:
        logger.warning("mm code %r: " % code + str(exc))
        raise nodes.SkipNode

    if self.builder.config.mermaid_pdfcrop != "":
        mm_args = [self.builder.config.mermaid_pdfcrop, outfn]
        try:
            p = Popen(mm_args, stdout=PIPE, stdin=PIPE, stderr=PIPE)
        except OSError as err:
            if err.errno != ENOENT:  # No such file or directory
                raise
            logger.warning(
                "command %r cannot be run (needed to crop pdf), \
                check the mermaid_cmd setting"
                % self.builder.config.mermaid_pdfcrop
            )
            return None, None

        stdout, stderr = p.communicate()
        if self.builder.config.mermaid_verbose:
            logger.info(stdout)

        if p.returncode != 0:
            raise MermaidError(
                "PdfCrop exited with error:\n[stderr]\n%s\n" "[stdout]\n%s" % (stderr, stdout)
            )
        if not os.path.isfile(outfn):
            raise MermaidError(
                "PdfCrop did not produce an output file:\n[stderr]\n%s\n"
                "[stdout]\n%s" % (stderr, stdout)
            )

        fname = "{filename[0]}-crop{filename[1]}".format(filename=os.path.splitext(fname))

    is_inline = self.is_inline(node)
    if is_inline:
        para_separator = ""
    else:
        para_separator = "\n"

    if fname is not None:
        post = None
        if not is_inline and "align" in node:
            if node["align"] == "left":
                self.body.append("{")
                post = "\\hspace*{\\fill}}"
            elif node["align"] == "right":
                self.body.append("{\\hspace*{\\fill}")
                post = "}"
        self.body.append(
            "%s\\sphinxincludegraphics{%s}%s" % (para_separator, fname, para_separator)
        )
        if post:
            self.body.append(post)

    raise nodes.SkipNode


def latex_visit_mermaid(self, node):
    render_mm_latex(self, node, node["code"], node["options"])


def render_mm_texinfo(self, node, code, options, prefix="mermaid"):
    try:
        fname, outfn = render_mm(self, code, options, "png", prefix)
    except MermaidError as exc:
        logger.warning("mm code %r: " % code + str(exc))
        raise nodes.SkipNode
    if fname is not None:
        self.body.append("@image{%s,,,[mermaid],png}\n" % fname[:-4])
    raise nodes.SkipNode


def texinfo_visit_mermaid(self, node):
    render_mm_texinfo(self, node, node["code"], node["options"])


def text_visit_mermaid(self, node):
    if "alt" in node.attributes:
        self.add_text(_("[graph: %s]") % node["alt"])
    else:
        self.add_text(_("[graph]"))
    raise nodes.SkipNode


def man_visit_mermaid(self, node):
    if "alt" in node.attributes:
        self.body.append(_("[graph: %s]") % node["alt"])
    else:
        self.body.append(_("[graph]"))
    raise nodes.SkipNode


def config_inited(app, config):
    version = config.mermaid_version
    mermaid_js_url = "https://unpkg.com/mermaid@{}/dist/mermaid.min.js".format(version)
    app.add_js_file(mermaid_js_url)
    app.add_js_file(
        None,
        body='mermaid.initialize({startOnLoad:true, theme:"neutral", securityLevel="loose", \
              sequenceConfig: {mirrorActors: false}});',
    )
    app.add_css_file("mermaid.css")


def on_build_finished(app: Sphinx, exc: Exception) -> None:
    if exc is None:
        src = os.path.join(os.path.dirname(__file__), "mermaid.css")
        dst = os.path.join(app.outdir, "_static")
        copy_asset(src, dst)


def setup(app):
    app.add_node(
        mermaid,
        html=(html_visit_mermaid, None),
        latex=(latex_visit_mermaid, None),
        texinfo=(texinfo_visit_mermaid, None),
        text=(text_visit_mermaid, None),
        man=(man_visit_mermaid, None),
    )
    app.add_directive("mermaid", Mermaid)

    #
    app.add_config_value("mermaid_cmd", "mmdc", "html")
    app.add_config_value("mermaid_pdfcrop", "", "html")
    app.add_config_value("mermaid_output_format", "raw", "html")
    app.add_config_value("mermaid_params", list(), "html")
    app.add_config_value("mermaid_verbose", False, "html")
    app.add_config_value("mermaid_sequence_config", False, "html")
    app.add_config_value("mermaid_version", "8.10.2", "html")

    app.connect("config-inited", config_inited)
    app.connect("build-finished", on_build_finished)

    return {"version": sphinx.__display_version__, "parallel_read_safe": True}
