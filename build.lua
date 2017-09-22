tool 'tools-gfx' {
   app 'atex' {
      icon 'icon/bengine-warm.ico',
      limp_src 'src-atex/*.hpp',
      src 'src-atex/*.cpp',
      link_project {
         'core',
         'core-id-with-names',
         'util',
         'util-fs',
         'util-string',
         'cli',
         'gfx-tex'
      }
   },
   app 'concur' {
      icon 'icon/bengine-warm.ico',
      limp_src 'src-concur/*.hpp',
      src 'src-concur/*.cpp',
      link_project {
         'core',
         'core-id-with-names',
         'util',
         'util-fs',
         'util-string',
         'cli',
         'ctable',
         'gfx'
      }
   }
}
