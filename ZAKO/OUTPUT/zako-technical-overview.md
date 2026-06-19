---
geometry:
  - top=24mm
  - bottom=22mm
  - left=22mm
  - right=20mm
mainfont: "Avenir Next"
monofont: "Menlo"
monofontoptions:
  - Scale=0.82
fontsize: 10pt
linestretch: 1.45
numbersections: true
toc: false
colorlinks: false
header-includes: |
  \usepackage{xcolor}
  \usepackage{fancyhdr}
  \usepackage{booktabs}
  \usepackage{longtable}
  \usepackage{colortbl}
  \usepackage{tabularx}
  \usepackage{microtype}
  \usepackage{parskip}
  \usepackage{listings}
  \usepackage{setspace}
  \usepackage{caption}
  \usepackage{amssymb}

  \definecolor{amber}{RGB}{168,100,0}
  \definecolor{amberbg}{RGB}{255,249,237}
  \definecolor{codebg}{RGB}{246,244,240}
  \definecolor{rulegray}{RGB}{210,205,195}
  \definecolor{mutedgray}{RGB}{105,100,92}
  \definecolor{textdark}{RGB}{18,16,22}

  \pagestyle{fancy}
  \fancyhf{}
  \renewcommand{\headrulewidth}{0.5pt}
  \renewcommand{\headrule}{\hbox to\headwidth{\color{rulegray}\leaders\hrule height \headrulewidth\hfill}}
  \fancyhead[L]{\small\color{mutedgray}\textbf{\textcolor{amber}{ZAKO}}\ \ Technical Overview v1.0}
  \fancyhead[R]{\small\color{mutedgray}\nouppercase{\leftmark}}
  \fancyfoot[L]{\footnotesize\color{mutedgray}ZAKO Standard v1.x\ \ \textperiodcentered\ \ June 2026}
  \fancyfoot[C]{\small\color{mutedgray}\thepage}
  \setlength{\headheight}{14pt}

  \renewcommand{\arraystretch}{1.3}
  \setlength{\tabcolsep}{6pt}

  \lstset{
    backgroundcolor=\color{codebg},
    basicstyle=\small\ttfamily\color{textdark},
    frame=l,
    framerule=2pt,
    rulecolor=\color{amber},
    xleftmargin=10pt,
    framexleftmargin=8pt,
    breaklines=true,
    columns=flexible,
    keepspaces=true,
    commentstyle=\itshape\color{mutedgray},
    aboveskip=8pt,
    belowskip=6pt,
    lineskip=1pt,
  }

  \newsavebox{\coutbox}
  \newenvironment{callout}{%
    \vspace{5pt}%
    \begin{lrbox}{\coutbox}%
      \begin{minipage}[t]{\dimexpr\linewidth-16pt\relax}%
      \vspace{4pt}\small%
  }{%
      \vspace{4pt}%
      \end{minipage}%
    \end{lrbox}%
    \noindent\colorbox{amberbg}{\usebox{\coutbox}}%
    \par\vspace{5pt}%
  }
