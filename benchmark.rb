require 'toml'

(1000000..100000000).step(1000000) do |n|
  doc = TOML::Generator.new({'input-size' => n, 'input-range' => n, 'seed' => 42}).body
  File.open('config.toml', 'w') do |f|
    f.write(doc)
  end
  puts "Benchmarking size #{n}..."
  system('./bench config.toml')
end
