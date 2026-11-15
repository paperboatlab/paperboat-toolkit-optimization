optimization
============

The optional optimization toolkit for Paperboat: linear, quadratic and nonlinear programs on the
Flow page, solved by `OSQP <https://osqp.org>`_ (Apache-2.0) and, optionally,
`Ipopt <https://coin-or.github.io/Ipopt/>`_.

.. toctree::
   :maxdepth: 1

   install
   problems
   blocks/index
   workspace

The step blocks are ``lp``, ``qp`` and ``nlp``, each with ``x``, ``objective``, ``iterations`` and
``status`` outputs. The path arrives at ``in``, the block solves once, and the path leaves by
``out``; the signal outputs carry the result on. A solve block is one step in a procedure (a
design point, a plan, a schedule); a new arrival re-reads the file and solves again.

Three examples ship with the toolkit, one per type: ``opt-01-beam-sizing`` (QP),
``opt-02-production-plan`` (LP) and ``opt-03-nonlinear-design`` (NLP).
