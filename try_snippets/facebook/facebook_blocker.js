const style = document.createElement('style');
style.textContent = `
  [data-dcm-id="1"] { display: none !important; }
  [data-srat="43"] { display: none !important; }
`;
document.head.appendChild(style);
console.log("Posts and stories hidden!");
